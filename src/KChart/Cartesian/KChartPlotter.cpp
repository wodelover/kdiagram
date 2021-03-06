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

#include "KChartPlotter.h"
#include "KChartPlotter_p.h"

#include "KChartAbstractGrid.h"
#include "KChartPainterSaver_p.h"
#include "KChartMath_p.h"

#include "KChartNormalPlotter_p.h"
#include "KChartPercentPlotter_p.h"
#include "KChartStackedPlotter_p.h"

using namespace KChart;

Plotter::Private::Private()
    : implementor( nullptr )
    , normalPlotter( nullptr )
    , percentPlotter( nullptr )
    , stackedPlotter( nullptr )
{
}

Plotter::Private::~Private()
{
    delete normalPlotter;
    delete percentPlotter;
    delete stackedPlotter;
}


#define d d_func()


Plotter::Plotter( QWidget* parent, CartesianCoordinatePlane* plane ) :
    AbstractCartesianDiagram( new Private(), parent, plane )
{
    init();
}

void Plotter::init()
{
    d->diagram = this;
    d->normalPlotter = new NormalPlotter( this );
    d->percentPlotter = new PercentPlotter( this );
    d->stackedPlotter = new StackedPlotter( this );
    d->implementor = d->normalPlotter;
    QObject* test = d->implementor->plotterPrivate();
    connect( this, SIGNAL(boundariesChanged()), test, SLOT(changedProperties()) );
    // The signal is connected to the superclass's slot at this point because the connection happened
    // in its constructor when "its type was not Plotter yet".
    disconnect( this, SIGNAL(attributesModelAboutToChange(AttributesModel*,AttributesModel*)),
                this, SLOT(connectAttributesModel(AttributesModel*)) );
    connect( this, SIGNAL(attributesModelAboutToChange(AttributesModel*,AttributesModel*)),
             this, SLOT(connectAttributesModel(AttributesModel*)) );
    setDatasetDimensionInternal( 2 );
}

Plotter::~Plotter()
{
}

Plotter* Plotter::clone() const
{
    Plotter* newDiagram = new Plotter( new Private( *d ) );
    newDiagram->setType( type() );
    return newDiagram;
}

bool Plotter::compare( const Plotter* other ) const
{
    if ( other == this )
        return true;
    if ( other == nullptr )
        return false;
    return  // compare the base class
            ( static_cast< const AbstractCartesianDiagram* >( this )->compare( other ) ) &&
            // compare own properties
            ( type() == other->type() );
}

void Plotter::connectAttributesModel( AttributesModel* newModel )
{
    // Order of setting the AttributesModel in compressor and diagram is very important due to slot
    // invocation order. Refer to the longer comment in
    // AbstractCartesianDiagram::connectAttributesModel() for details.

    if ( useDataCompression() == Plotter::NONE )
    {
        d->plotterCompressor.setModel( nullptr );
        AbstractCartesianDiagram::connectAttributesModel( newModel );
    }
    else
    {
        d->compressor.setModel( nullptr );
        if ( attributesModel() != d->plotterCompressor.model() )
        {
            d->plotterCompressor.setModel( attributesModel() );
            connect( &d->plotterCompressor, SIGNAL(boundariesChanged()), this, SLOT(setDataBoundariesDirty()) );
            if ( useDataCompression() != Plotter::SLOPE )
            {
                connect( coordinatePlane(), SIGNAL(internal_geometryChanged(QRect,QRect)),
                         this, SLOT(setDataBoundariesDirty()) );
                connect( coordinatePlane(), SIGNAL(geometryChanged(QRect,QRect)),
                         this, SLOT(setDataBoundariesDirty()) );
                calcMergeRadius();
            }
        }
    }
}

Plotter::CompressionMode Plotter::useDataCompression() const
{    
    return d->implementor->useCompression();
}

void Plotter::setUseDataCompression( Plotter::CompressionMode value )
{
    if ( useDataCompression() != value )
    {
        d->implementor->setUseCompression( value );
        if ( useDataCompression() != Plotter::NONE )
        {
            d->compressor.setModel( nullptr );
            if ( attributesModel() != d->plotterCompressor.model() )                
                d->plotterCompressor.setModel( attributesModel() );
        }
    }
}

qreal Plotter::maxSlopeChange() const
{
    return d->plotterCompressor.maxSlopeChange();
}

void Plotter::setMaxSlopeChange( qreal value )
{
    d->plotterCompressor.setMaxSlopeChange( value );
}

qreal Plotter::mergeRadiusPercentage() const
{
    return d->mergeRadiusPercentage;
}

void Plotter::setMergeRadiusPercentage( qreal value )
{
    if ( d->mergeRadiusPercentage != value )
    {
        d->mergeRadiusPercentage = value;
        //d->plotterCompressor.setMergeRadiusPercentage( value );
        //update();
    }
}

void Plotter::setType( const PlotType type )
{
    if ( d->implementor->type() == type ) {
        return;
    }
    if ( datasetDimension() != 2 )  {
        Q_ASSERT_X ( false, "setType()",
                     "This line chart type can only be used with two-dimensional data." );
        return;
    }
    switch ( type ) {
    case Normal:
        d->implementor = d->normalPlotter;
        break;
    case Percent:
        d->implementor = d->percentPlotter;
        break;
    case Stacked:
        d->implementor = d->stackedPlotter;
        break;
    default:
        Q_ASSERT_X( false, "Plotter::setType", "unknown plotter subtype" );
    }
    bool connection = connect( this, SIGNAL(boundariesChanged()),
                               d->implementor->plotterPrivate(), SLOT(changedProperties()) );
    Q_ASSERT( connection );
    Q_UNUSED( connection );

    // d->lineType = type;
    Q_ASSERT( d->implementor->type() == type );

    setDataBoundariesDirty();
    emit layoutChanged( this );
    emit propertiesChanged();
}

Plotter::PlotType Plotter::type() const
{
    return d->implementor->type();
}

void Plotter::setLineAttributes( const LineAttributes& la )
{
    d->attributesModel->setModelData( QVariant::fromValue( la ), LineAttributesRole );
    emit propertiesChanged();
}

void Plotter::setLineAttributes( int column, const LineAttributes& la )
{
    d->setDatasetAttrs( column, QVariant::fromValue( la ), LineAttributesRole );
    emit propertiesChanged();
}

void Plotter::resetLineAttributes( int column )
{
    d->resetDatasetAttrs( column, LineAttributesRole );
    emit propertiesChanged();
}

void Plotter::setLineAttributes( const QModelIndex & index, const LineAttributes& la )
{
    d->attributesModel->setData( d->attributesModel->mapFromSource( index ),
                                 QVariant::fromValue( la ), LineAttributesRole );
    emit propertiesChanged();
}

void Plotter::resetLineAttributes( const QModelIndex & index )
{
    d->attributesModel->resetData(
            d->attributesModel->mapFromSource(index), LineAttributesRole );
    emit propertiesChanged();
}

LineAttributes Plotter::lineAttributes() const
{
    return d->attributesModel->data( KChart::LineAttributesRole ).value<LineAttributes>();
}

LineAttributes Plotter::lineAttributes( int column ) const
{
    const QVariant attrs( d->datasetAttrs( column, LineAttributesRole ) );
    if ( attrs.isValid() )
        return attrs.value<LineAttributes>();
    return lineAttributes();
}

LineAttributes Plotter::lineAttributes( const QModelIndex& index ) const
{
    return d->attributesModel->data(
        d->attributesModel->mapFromSource( index ), KChart::LineAttributesRole ).value<LineAttributes>();
}

void Plotter::setThreeDLineAttributes( const ThreeDLineAttributes& la )
{
    setDataBoundariesDirty();
    d->attributesModel->setModelData( QVariant::fromValue( la ), ThreeDLineAttributesRole );
    emit propertiesChanged();
}

void Plotter::setThreeDLineAttributes( int column, const ThreeDLineAttributes& la )
{
    setDataBoundariesDirty();
    d->setDatasetAttrs( column, QVariant::fromValue( la ), ThreeDLineAttributesRole );
    emit propertiesChanged();
}

void Plotter::setThreeDLineAttributes( const QModelIndex& index, const ThreeDLineAttributes& la )
{
    setDataBoundariesDirty();
    d->attributesModel->setData( d->attributesModel->mapFromSource( index ), QVariant::fromValue( la ),
                                 ThreeDLineAttributesRole );
    emit propertiesChanged();
}

ThreeDLineAttributes Plotter::threeDLineAttributes() const
{
    return d->attributesModel->data( KChart::ThreeDLineAttributesRole ).value<ThreeDLineAttributes>();
}

ThreeDLineAttributes Plotter::threeDLineAttributes( int column ) const
{
    const QVariant attrs( d->datasetAttrs( column, ThreeDLineAttributesRole ) );
    if ( attrs.isValid() ) {
        return attrs.value<ThreeDLineAttributes>();
    }
    return threeDLineAttributes();
}

ThreeDLineAttributes Plotter::threeDLineAttributes( const QModelIndex& index ) const
{
    return d->attributesModel->data(
        d->attributesModel->mapFromSource( index ), KChart::ThreeDLineAttributesRole ).value<ThreeDLineAttributes>();
}

qreal Plotter::threeDItemDepth( const QModelIndex & index ) const
{
    return threeDLineAttributes( index ).validDepth();
}

qreal Plotter::threeDItemDepth( int column ) const
{
    return threeDLineAttributes( column ).validDepth();
}

void Plotter::setValueTrackerAttributes( const QModelIndex & index, const ValueTrackerAttributes & va )
{
    d->attributesModel->setData( d->attributesModel->mapFromSource( index ),
                                 QVariant::fromValue( va ), KChart::ValueTrackerAttributesRole );
    emit propertiesChanged();
}

ValueTrackerAttributes Plotter::valueTrackerAttributes( const QModelIndex & index ) const
{
    return d->attributesModel->data(
        d->attributesModel->mapFromSource( index ), KChart::ValueTrackerAttributesRole ).value<ValueTrackerAttributes>();
}

void Plotter::resizeEvent ( QResizeEvent* )
{
}

const QPair< QPointF, QPointF > Plotter::calculateDataBoundaries() const
{
    if ( !checkInvariants( true ) )
        return QPair< QPointF, QPointF >( QPointF( 0, 0 ), QPointF( 0, 0 ) );

    // note: calculateDataBoundaries() is ignoring the hidden flags.
    //       That's not a bug but a feature: Hiding data does not mean removing them.
    // For totally removing data from KD Chart's view people can use e.g. a proxy model ...

    // calculate boundaries for different line types Normal - Stacked - Percent - Default Normal
    return d->implementor->calculateDataBoundaries();
}


void Plotter::paintEvent ( QPaintEvent*)
{
    QPainter painter ( viewport() );
    PaintContext ctx;
    ctx.setPainter ( &painter );
    ctx.setRectangle ( QRectF ( 0, 0, width(), height() ) );
    paint ( &ctx );
}

void Plotter::paint( PaintContext* ctx )
{
    // note: Not having any data model assigned is no bug
    //       but we can not draw a diagram then either.
    if ( !checkInvariants( true ) ) return;

    AbstractCoordinatePlane* const plane = ctx->coordinatePlane();
    if ( ! plane ) return;
    d->setCompressorResolution( size(), plane );

    if ( !AbstractGrid::isBoundariesValid(dataBoundaries()) ) return;

    const PainterSaver p( ctx->painter() );
    if ( model()->rowCount( rootIndex() ) == 0 || model()->columnCount( rootIndex() ) == 0 )
        return; // nothing to paint for us

    ctx->setCoordinatePlane( plane->sharedAxisMasterPlane( ctx->painter() ) );

    // paint different line types Normal - Stacked - Percent - Default Normal
    d->implementor->paint( ctx );

    ctx->setCoordinatePlane( plane );
}

void Plotter::resize ( const QSizeF& size )
{
    d->setCompressorResolution( size, coordinatePlane() );
    if ( useDataCompression() == Plotter::BOTH || useDataCompression() == Plotter::DISTANCE )
    {
        d->plotterCompressor.cleanCache();
        calcMergeRadius();
    }
    setDataBoundariesDirty();
    AbstractCartesianDiagram::resize( size );
}

void Plotter::setDataBoundariesDirty()
{
    AbstractCartesianDiagram::setDataBoundariesDirty();
    if ( useDataCompression() == Plotter::DISTANCE || useDataCompression() == Plotter::BOTH )
    {
        calcMergeRadius();
        //d->plotterCompressor.setMergeRadiusPercentage( d->mergeRadiusPercentage );
    }
}

void Plotter::calcMergeRadius()
{
    CartesianCoordinatePlane *plane = dynamic_cast< CartesianCoordinatePlane* >( coordinatePlane() );
    Q_ASSERT( plane );
    //Q_ASSERT( plane->translate( plane->translateBack( plane->visibleDiagramArea().topLeft() ) ) == plane->visibleDiagramArea().topLeft() );
    QRectF range = plane->visibleDataRange();
    //qDebug() << range;
    const qreal radius = std::sqrt( ( range.x() + range.width() ) * ( range.y() +  range.height() ) );
    //qDebug() << radius;
    //qDebug() << radius * d->mergeRadiusPercentage;
    //qDebug() << d->mergeRadiusPercentage;
    d->plotterCompressor.setMergeRadius( radius * d->mergeRadiusPercentage );
}

#if defined(Q_COMPILER_MANGLES_RETURN_TYPE)
const
#endif
int Plotter::numberOfAbscissaSegments () const
{
    return d->attributesModel->rowCount( attributesModelRootIndex() );
}

#if defined(Q_COMPILER_MANGLES_RETURN_TYPE)
const
#endif
int Plotter::numberOfOrdinateSegments () const
{
    return d->attributesModel->columnCount( attributesModelRootIndex() );
}
