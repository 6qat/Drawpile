/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2014-2016 Calle Laakkonen

   Drawpile is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Drawpile is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Drawpile.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <QDebug>
#include <QVector>
#include <QHash>
#include <QSet>

#include "../shared/record/reader.h"
#include "../shared/record/writer.h"
#include "../shared/net/undo.h"
#include "../shared/net/recording.h"

#include "filter.h"

namespace recording {

namespace {

static const uchar UNDOABLE = (1<<7);
static const uchar REMOVED = (1<<6);

struct FilterIndex {
	uchar type;
	uchar ctxid;
	uchar flags;
	qint64 offset;

	FilterIndex() : type(0), ctxid(0), flags(0), offset(0) { }
};

struct State {
	QVector<FilterIndex> index;
	QHash<int, QList<int>> userjoins;
	QHash<quint32, protocol::Message*> replacements;
	QSet<int> users_seen;

	~State() {
		for(protocol::Message* msg : replacements.values())
			delete msg;
	}
};

void mark_delete(FilterIndex &i) {
	i.flags |= REMOVED;
}

inline protocol::MessageUndoState undostate(const FilterIndex &i) {
	return protocol::MessageUndoState(i.flags & 0x03);
}

inline bool isUndoable(const FilterIndex &i) {
	return i.flags & UNDOABLE;
}

bool isDeleted(const FilterIndex &i) {
	return i.flags & (REMOVED | protocol::UNDONE);
}

void filterMessage(const Filter &filter, State &state, protocol::MessagePtr msg, qint64 offset)
{
	// Put this message in the index
	{
		FilterIndex imsg;
		imsg.type = msg->type();
		imsg.ctxid = msg->contextId();
		imsg.flags = msg->isUndoable() ? UNDOABLE : 0;
		imsg.offset = offset;
		state.index.append(imsg);
	}

	// Filter out select message types
	switch(msg->type()) {
	using namespace protocol;
	case MSG_CHAT:
		if(filter.removeChat()) {
			mark_delete(state.index.last());
			return;
		}
		break;

	case MSG_USER_JOIN:
	case MSG_USER_LEAVE:
		state.userjoins[msg->contextId()].append(state.index.size()-1);
		return;

	case MSG_INTERVAL:
		if(filter.removeDelays()) {
			mark_delete(state.index.last());
			return;
		}
		break;

	case MSG_MOVEPOINTER:
		if(filter.removeLasers()) {
			mark_delete(state.index.last());
			return;
		}
		break;

	case MSG_MARKER:
		if(filter.removeMarkers()) {
			mark_delete(state.index.last());
			return;
		}
		break;

	default: break;
	}

	// Perform undo
	if(msg->type() == protocol::MSG_UNDO && filter.expungeUndos()) {
		// Normally, performing an undo will implicitly delete the undo action itself,
		// but it can be restored by a redo. Therefore, we must explicitly flag the
		// undo messages for deletion to be sure they are gone.
		mark_delete(state.index.last());

		// Perform an undo. This is a stripped down version of handleUndo from StateTracker
		const protocol::Undo &cmd = msg.cast<protocol::Undo>();

		const uchar ctxid = cmd.contextId();

		// Step 1. Find undo or redo point
		int pos = state.index.size();
		int upCount = 0;

		if(cmd.isRedo()) {
			// Find the start of the undo sequence (oldest undone UndoPoint)
			int redostart = pos;
			while(--pos>=0 && upCount <= protocol::UNDO_DEPTH_LIMIT) {
				const FilterIndex u = state.index[pos];
				if(u.type == protocol::MSG_UNDOPOINT) {
					++upCount;
					if(u.ctxid == ctxid) {
						if(undostate(u) != protocol::DONE)
							redostart = pos;
						else
							break;
					}
				}
			}

			if(redostart == state.index.size()) {
				qDebug() << "nothing to redo for user" << cmd.contextId();
				mark_delete(state.index.last());
				return;
			}
			pos = redostart;

		} else {
			// Search for undoable actions from the end of the
			// command stream towards the beginning
			while(--pos>=0 && upCount <= protocol::UNDO_DEPTH_LIMIT) {
				const FilterIndex u = state.index[pos];

				if(u.type == protocol::MSG_UNDOPOINT) {
					++upCount;
					if(u.ctxid == ctxid && undostate(u) == protocol::DONE)
						break;
				}
			}
		}

		if(upCount > protocol::UNDO_DEPTH_LIMIT) {
			qDebug() << "user" << cmd.contextId() << "cannot undo/redo beyond history limit";
			mark_delete(state.index.last());
			return;
		}

		// Step 2 is not needed here

		// Step 3. (Un)mark all actions by the user as undone
		if(cmd.isRedo()) {
			int i=pos;
			int sequence=2;
			while(i<state.index.size()) {
				FilterIndex &u = state.index[i];
				if(u.ctxid == ctxid) {
					if(u.type == protocol::MSG_UNDOPOINT && undostate(u) != protocol::GONE)
						if(--sequence==0)
							break;

					// GONE messages cannot be redone
					if(undostate(u) == protocol::UNDONE)
						u.flags &= ~protocol::UNDONE;
				}
				++i;
			}
		} else {
			for(int i=pos;i<state.index.size();++i) {
				FilterIndex &u = state.index[i];
				if(u.ctxid == ctxid && isUndoable(u))
					u.flags |= protocol::UNDONE;
			}
		}
		// Steps 4 is not needed here.
	}

	if(msg->contextId()>0)
		state.users_seen.insert(msg->contextId());

	return;
}

//! Remove all users who haven't done anything
void filterLookyloos(State &state)
{
	for(const int id : state.userjoins.keys()) {
		if(!state.users_seen.contains(id)) {
			for(const int idx : state.userjoins[id]) {
				mark_delete(state.index[idx]);
			}
		}
	}
}

/**
 * @brief Remove adjacent undo points by the same user
 *
 * This removes all duplicate undo points that appear _next to each other_ in
 * the message stream. Such undo points are left over when actions are undone or
 * silenced.
 *
 * @param state
 */
void filterAdjacentUndoPoints(State &state)
{
	FilterIndex prev;

	for(int i=0;i<state.index.size();++i) {
		FilterIndex &fi = state.index[i];

		if(isDeleted(fi))
			continue;

		if(fi.type == protocol::MSG_INTERVAL)
			continue;

		if(fi.type == protocol::MSG_UNDOPOINT && prev.type == protocol::MSG_UNDOPOINT) {
			if(fi.ctxid == prev.ctxid) {
				mark_delete(fi);
			}
		}

		prev = fi;
	}
}

void squishStrokes(State &state, Reader &recording)
{
#if 0 // TODO this needs to be reimplemented for DrawDabs*
	QHash<int,int> strokes; // ctxId -> current stroke mapping

	for(int i=0;i<state.index.size();++i) {
		FilterIndex &fi = state.index[i];

		if(isDeleted(fi))
			continue;

		if(fi.type == protocol::MSG_PEN_MOVE) {
			recording.seekTo(i, fi.offset);
			MessageRecord msg = recording.readNext();
			Q_ASSERT(msg.status == MessageRecord::OK);
			Q_ASSERT(msg.message->type() == protocol::MSG_PEN_MOVE);

			if(strokes.contains(fi.ctxid)) {
				// a stroke is still underway: add coordinates to the replacement PenMove
				protocol::Message *squished = state.replacements[strokes[fi.ctxid]];
				Q_ASSERT(squished);
				Q_ASSERT(squished->type() == protocol::MSG_PEN_MOVE);

				protocol::PenMove *pm = static_cast<protocol::PenMove*>(squished);
				protocol::PenMove *pm2 = static_cast<protocol::PenMove*>(msg.message);
				if(pm->points().size() + pm2->points().size() > protocol::PenMove::MAX_POINTS) {
					// Maximum points per message reached! Continue in a new message
					strokes[fi.ctxid] = i;
					state.replacements[i] = msg.message;

				} else {
					pm->points() += pm2->points();
					mark_delete(fi);
					delete msg.message;
				}

			} else {
				// start a new stroke
				strokes[fi.ctxid] = i;
				state.replacements[i] = msg.message;
			}

		} else if(fi.type == protocol::MSG_PEN_UP) {
			strokes.remove(fi.ctxid);
		}
	}
#endif
}

void doFilterRecording(Filter &filter, State &state, Reader &recording)
{
	while(true) {
		MessageRecord msg = recording.readNext();
		if(msg.status == MessageRecord::END_OF_RECORDING)
			break;
		if(msg.status == MessageRecord::INVALID) {
			qWarning() << "skipping invalid message type" << msg.error.type;
			continue;
		}

		protocol::MessagePtr msgp(msg.message);
		filterMessage(filter, state, msgp, recording.currentPosition());
	}

	if(filter.removeLookyloos())
		filterLookyloos(state);

	if(filter.squishStrokes())
		squishStrokes(state, recording);

	filterAdjacentUndoPoints(state);
}

}

Filter::Filter()
	: _expunge_undos(false), _remove_chat(false), _remove_lookyloos(false), _remove_delays(false),
	  _remove_lasers(false), _remove_markers(false)
{
}

bool Filter::filterRecording(const QString &input, const QString &outputfile)
{
	// Step 1. Open input and output files
	Reader reader1(input);
	Compatibility readOk = reader1.open();
	if(readOk != COMPATIBLE && readOk != MINOR_INCOMPATIBILITY) {
		qWarning() << "Cannot open recording for filtering. Error code" << readOk;
		_errormsg = reader1.errorString();
		return false;
	}

	Writer writer(outputfile);
	if(!writer.open()) {
		qWarning() << "Cannot open record filter output file";
		_errormsg = writer.errorString();
		return false;
	}

	// Step 2. Mark messages for removal
	State state;
	doFilterRecording(*this, state, reader1);

	// Step 4. Write messages back to output file
#if 0
	reader.rewind();
#else
	// KCompressionDevice::seek appears to be buggy
	reader1.close();
	Reader reader2(input);
	reader2.open();
#endif

	writer.writeHeader();

	QByteArray buffer;
	while(reader2.readNextToBuffer(buffer)) {
		const unsigned int pos = reader2.currentIndex();

		// Copy (or replace) original message, unless marked for deletion
		if(!isDeleted(state.index[pos])) {

			if(state.replacements.contains(pos))
				writer.writeMessage(*state.replacements[pos]);
			else
				writer.writeFromBuffer(buffer);
		}
	}

	writer.close();

	return true;
}

}
