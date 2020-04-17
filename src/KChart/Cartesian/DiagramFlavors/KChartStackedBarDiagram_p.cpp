/*
 * Copyright (C) 2001-2015 Klaralvdalens Datakonsult AB.  All rights reserved.
 *
 * This file is part of the KD Chart library.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "KChartStackedBarDiagram_p.h"
#include <QModelIndex>

#include "KChartBarDiagram.h"
#include "KChartTextAttributes.h"
#include "KChartAttributesModel.h"
#include "KChartAbstractCartesianDiagram.h"

using namespace KChart;

StackedBarDiagram::StackedBarDiagram( BarDiagram* d )
    : BarDiagramType( d )
{
}

BarDiagram::BarType StackedBarDiagram::type() const
{
    return BarDiagram::Stacked;
}

const QPair<QPointF, QPointF> StackedBarDiagram::calculateDataBoundaries() const
{
    const int rowCount = compressor().modelDataRows();
    const int colCount = compressor().modelDataColumns();

    const qreal xMin = 0.0;
    const qreal xMax = rowCount;
    qreal yMin = 0.0;
    qreal yMax = 0.0;

    bool isFirst = true;
    for ( int row = 0; row < rowCount; ++row ) {
        // calculate sum of values per column - Find out stacked Min/Max
        qreal stackedValues = 0.0;
        qreal negativeStackedValues = 0.0;
        for ( int col = 0; col < colCount; ++col ) {
            const CartesianDiagramDataCompressor::CachePosition position( row, col );
            const CartesianDiagramDataCompressor::DataPoint point = compressor().data( position );
            const double value = ISNAN( point.value ) ? 0.0 : point.value;

            if ( value > 0.0 ) {
                stackedValues += value;
            } else {
                negativeStackedValues += value;
            }

            // this is always true yMin can be 0 in case all values
            // are the same
            // same for yMax it can be zero if all values are negative
            if ( isFirst ) {
                yMin = negativeStackedValues < 0.0 ? negativeStackedValues : stackedValues;
                yMax = stackedValues > 0.0 ? stackedValues : negativeStackedValues;
                isFirst = false;
            } else {
                yMin = qMin( qMin( yMin, stackedValues ), negativeStackedValues );
                yMax = qMax( qMax( yMax, stackedValues ), negativeStackedValues );
            }
        }
    }

    // special cases
    if ( yMax == yMin ) {
        if ( yMin == 0.0 ) {
            yMax = 0.1; // we need at least a range
        } else if ( yMax < 0.0 ) {
            yMax = 0.0; // extend the range to zero
        } else if ( yMin > 0.0 ) {
            yMin = 0.0; // dito
        }
    }

    return QPair< QPointF, QPointF >( QPointF( xMin, yMin ), QPointF( xMax, yMax ) );
}

void StackedBarDiagram::paint( PaintContext* ctx )
{
    reverseMapper().clear();

    const QPair<QPointF,QPointF> boundaries = diagram()->dataBoundaries(); // cached

    const QPointF boundLeft = ctx->coordinatePlane()->translate( boundaries.first ) ;
    const QPointF boundRight = ctx->coordinatePlane()->translate( boundaries.second );

    const int rowCount = compressor().modelDataRows();
    const int colCount = compressor().modelDataColumns();

    BarAttributes ba = diagram()->barAttributes();
    qreal barWidth = 0;
    qreal maxDepth = 0;
    qreal width = boundRight.x() - boundLeft.x();
    qreal groupWidth = width / rowCount;
    qreal spaceBetweenBars = 0;
    qreal spaceBetweenGroups = 0;

    if ( ba.useFixedBarWidth() ) {
        barWidth = ba.fixedBarWidth();
        groupWidth += barWidth;

        // Pending Michel set a min and max value for the groupWidth
        // related to the area.width
        if ( groupWidth < 0 )
            groupWidth = 0;

        if ( groupWidth  * rowCount > width )
            groupWidth = width / rowCount;
    }

    // maxLimit: allow the space between bars to be larger until area.width()
    // is covered by the groups.
    qreal maxLimit = rowCount * (groupWidth + ((colCount-1) * ba.fixedDataValueGap()) );


    //Pending Michel: FixMe
    if ( ba.useFixedDataValueGap() ) {
        if ( width > maxLimit )
            spaceBetweenBars += ba.fixedDataValueGap();
        else
            spaceBetweenBars = ((width/rowCount) - groupWidth)/(colCount-1);
    }

    if ( ba.useFixedValueBlockGap() )
        spaceBetweenGroups += ba.fixedValueBlockGap();

    calculateValueAndGapWidths( rowCount, colCount,groupWidth,
                                barWidth, spaceBetweenBars, spaceBetweenGroups );

    LabelPaintCache lpc;
    for ( int col = 0; col < colCount; ++col )
    {
        qreal offset = spaceBetweenGroups;
        if ( ba.useFixedBarWidth() )
            offset -= ba.fixedBarWidth();

        CartesianCoordinatePlane *plane = static_cast<CartesianCoordinatePlane*>(ctx->coordinatePlane());
        if (plane->isHorizontalRangeReversed()) {
            if (offset > 0) {
                offset = 0;
            }
        } else if ( offset < 0 ) {
            offset = 0;
        }
        for ( int row = 0; row < rowCount; ++row )
        {
            const CartesianDiagramDataCompressor::CachePosition position( row, col );
            const CartesianDiagramDataCompressor::DataPoint p = compressor().data( position );
 
            const QModelIndex index = attributesModel()->mapToSource( p.index );
            ThreeDBarAttributes threeDAttrs = diagram()->threeDBarAttributes( index );
            const qreal value = p.value;
            qreal stackedValues = 0.0;
            qreal key = 0.0;

            if ( threeDAttrs.isEnabled() ) {
                if ( barWidth > 0 )
                    barWidth =  (width - ((offset+(threeDAttrs.depth()))*rowCount))/ rowCount;
                if ( barWidth <= 0 ) {
                    barWidth = 0;
                    maxDepth = offset - (width/rowCount);
                }
            } else {
                barWidth =  (width - (offset*rowCount))/ rowCount ;
            }

            for ( int k = col; k >= 0; --k )
            {
                const CartesianDiagramDataCompressor::CachePosition position( row, k );
                const CartesianDiagramDataCompressor::DataPoint point = compressor().data( position );
                if ( !ISNAN( point.value ) && (( p.value >= 0.0 && point.value >= 0.0 ) || ( p.value < 0.0 && point.value < 0.0 )) )
                    stackedValues += point.value;
                key = point.key;
            }

            if (!ISNAN( value ))
            {
                const qreal usedDepth = threeDAttrs.depth();

                QPointF point = ctx->coordinatePlane()->translate( QPointF( key, stackedValues ) );

                const qreal dy = point.y() - usedDepth;
                if ( dy < 0 ) {
                    threeDAttrs.setDepth( point.y() - 1 );
                    diagram()->setThreeDBarAttributes( threeDAttrs );
                }

                point.rx() += offset / 2;
                const QPointF previousPoint = ctx->coordinatePlane()->translate( QPointF( key, stackedValues - value ) );
                const qreal barHeight = previousPoint.y() - point.y();

                const QRectF rect( point, QSizeF( barWidth , barHeight ) );
                m_private->addLabel( &lpc, index, nullptr, PositionPoints( rect ), Position::North,
                                     Position::South, value );
                paintBars( ctx, index, rect, maxDepth );
            }
        }
    }
    m_private->paintDataValueTextsAndMarkers( ctx, lpc, false );
}
