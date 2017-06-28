/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2015-2017 Calle Laakkonen

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

#ifndef SELECTION_H
#define SELECTION_H

#include "core/blendmodes.h"
#include "../shared/net/message.h"

#include <QObject>
#include <QImage>
#include <QPolygonF>

namespace canvas {

/**
 * @brief This class represents a canvas area selection
 *
 * A selection can contain in image to be pasted.
 */
class Selection : public QObject
{
	Q_PROPERTY(QPolygonF shape READ shape WRITE setShape NOTIFY shapeChanged)
	Q_PROPERTY(QImage pasteImage READ pasteImage WRITE setPasteImage NOTIFY pasteImageChanged)
	Q_PROPERTY(int handleSize READ handleSize CONSTANT)
	Q_OBJECT
public:
	enum Handle {OUTSIDE, TRANSLATE, RS_TOPLEFT, RS_TOPRIGHT, RS_BOTTOMRIGHT, RS_BOTTOMLEFT, RS_TOP, RS_RIGHT, RS_BOTTOM, RS_LEFT};

	explicit Selection(QObject *parent=nullptr);

	//! Get the selection shape polygon
	QPolygonF shape() const { return m_shape; }

	//! Set the selection shape polygon. The shape is not closed.
	void setShape(const QPolygonF &shape);

	//! Set the selection shape from a rectangle. The shape is closed.
	void setShapeRect(const QRect &rect);

	//! Add a point to the selection polygon. The shape must be open
	void addPointToShape(const QPointF &point);

	//! Close the selection
	void closeShape();

	bool isClosed() const { return m_closedPolygon; }

	Handle handleAt(const QPointF &point, float zoom) const;
	void adjustGeometry(Handle handle, const QPoint &delta, bool keepAspect);

	bool isAxisAlignedRectangle() const;

	//! Forget that the selection buffer came from the canvas (will be treated as a pasted image)
	void detachMove() { m_moveRegion = QPolygon(); }

	//! Did the selection buffer come from the canvas? (as opposed to an external pasted image)
	bool isMovedFromCanvas() const { return !m_moveRegion.isEmpty(); }

	QRect boundingRect() const { return m_shape.boundingRect().toRect(); }

	QImage shapeMask(const QColor &color, QRect *maskBounds) const;

	//! Set the image from pasting
	void setPasteImage(const QImage &image);

	/**
	 * @brief Set the preview image for moving a layer region
	 *
	 * The current shape will be remembered.
	 * @param image
	 * @param canvasSize the size of the canvas (selection rectangle is clipped to canvas bounds
	 */
	void setMoveImage(const QImage &image, const QSize &canvasSize);

	//! Get the image to be pasted (or the move preview)
	QImage pasteImage() const { return m_pasteImage; }

	/**
	 * @brief Apply changes to the canvas.
	 *
	 * If this selection contains a moved piece (setMoveImage called) calling this method
	 * will return the commands for a RegionMove.
	 * If the image came from outside (clipboard,) a PutImage command set will be returned
	 * instead.
	 *
	 * @param contextId user ID for the commands
	 * @param layer target layer
	 * @return set of commands
	 */
	QList<protocol::MessagePtr> pasteOrMoveToCanvas(uint8_t contextId, int layer) const;
	QList<protocol::MessagePtr> fillCanvas(uint8_t contextId, const QColor &color, paintcore::BlendMode::Mode mode, int layer) const;

	int handleSize() const { return 20; }

	//! Has the selection been moved or transformed?
	bool isTransformed() const;

public slots:
	//! Restore the original shape (but not the position)
	void resetShape();

	//! Restore the original shape and position
	void reset();

	void translate(const QPoint &offset);
	void scale(qreal x, qreal y);
	void rotate(float angle);
	void shear(float sh, float sv);

signals:
	void shapeChanged(const QPolygonF &shape);
	void pasteImageChanged(const QImage &image);
	void closed();

private:
	void setPasteOrMoveImage(const QImage &image);
	void adjust(int dx1, int dy1, int dx2, int dy2);
	void saveShape();

	QPolygonF m_shape;
	QPolygonF m_originalShape;
	QPointF m_originalCenter;

	QImage m_pasteImage;

	bool m_closedPolygon;
	QPolygonF m_moveRegion;
};

}

#endif // SELECTION_H
