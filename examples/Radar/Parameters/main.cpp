/**
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
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <QStandardItemModel>
#include <KChartChart>
#include <KChartRadarDiagram>
#include <KChartDataValueAttributes>
#include <KChartBackgroundAttributes>

#include <QApplication>

using namespace KChart;

class ChartWidget : public QWidget {
    Q_OBJECT
public:
    explicit ChartWidget(QWidget* parent=0)
        : QWidget(parent)
    {

        // initialize the ItemModel and fill in some data
        m_model.insertRows( 0, 10 );
        m_model.insertColumns(  0,  5 );
        int value = 0;
        for ( int column = 0; column < m_model.columnCount(); ++column ) {
            for ( int row = 0; row < m_model.rowCount(); ++row ) {
                QModelIndex index = m_model.index( row, column );
                m_model.setData( index, QVariant( value++  ) );
            }
        }
        // We need a Radar plane for the Radar type
        RadarCoordinatePlane* plane = new RadarCoordinatePlane( &m_chart );
        // replace the default Cartesian plane with
        // our Radar plane
        m_chart.replaceCoordinatePlane( plane );
    
        TextAttributes tt = plane->textAttributes();
        tt.setPen(QPen(QColor(Qt::yellow), 2.0));
        plane->setTextAttributes(tt);

        // assign the model to our polar diagram
        RadarDiagram* diagram = new RadarDiagram;
        diagram->setModel(&m_model);

        // Configure the plane's Background
        BackgroundAttributes pba;
        pba.setBrush( QBrush(QColor(0x10,0x10,0xA0)) );
        pba.setVisible( true );
        plane->setBackgroundAttributes(  pba );


        // Configure some global / dataset / cell specific attributes:

        DataValueAttributes dva( diagram->dataValueAttributes() );
        MarkerAttributes ma( dva.markerAttributes() );
        ma.setVisible( true );
        ma.setMarkerStyle( MarkerAttributes::MarkerSquare );
        ma.setMarkerSize( QSize( 6,6 ) );
        dva.setMarkerAttributes( ma );
        // find a nicer place for the data value texts:
        // We want them to be centered on top of their respective markers.
        RelativePosition relativePosition( dva.positivePosition() );
        relativePosition.setReferencePosition( Position::Center );
        relativePosition.setAlignment( Qt::AlignBottom | Qt::AlignHCenter );
        relativePosition.setHorizontalPadding( KChart::Measure( 0.0, KChartEnums::MeasureCalculationModeAbsolute ) );
        relativePosition.setVerticalPadding(   KChart::Measure( 0.0, KChartEnums::MeasureCalculationModeAbsolute ) );
        dva.setPositivePosition( relativePosition );
        diagram->setDataValueAttributes( dva );

        // Display data values
        const QFont font(QFont( "Comic", 10 ));
        const int colCount = diagram->model()->columnCount();
        for ( int iColumn = 0; iColumn<colCount; ++iColumn ) {
            DataValueAttributes dva( diagram->dataValueAttributes( iColumn ) );
            TextAttributes ta( dva.textAttributes() );
            ta.setRotation( 0 );
            ta.setFont( font );
            ta .setPen( QPen( Qt::gray ) );
            ta.setVisible( true );
            dva.setTextAttributes( ta );
            dva.setVisible( true );
            diagram->setDataValueAttributes( iColumn, dva);
        }


        // Set the marker of one single cell differently to show
        // how per-cell marker attributes can be used:
        const QModelIndex index = diagram->model()->index( 1, 2, QModelIndex() );
        dva = diagram->dataValueAttributes( index );
        ma = dva.markerAttributes();
        ma.setMarkerStyle( MarkerAttributes::MarkerCircle );
        ma.setMarkerSize( QSize( 40,40 ) );

        // This is the canonical way to adjust a marker's color:
        // By default the color is invalid so we use an explicit fallback
        // here to make sure we are getting the right color, as it would
        // be used by KD Chart's built-in logic too:
        QColor semiTrans( ma.markerColor() );
        if ( ! semiTrans.isValid() )
            semiTrans = diagram->brush( index ).color();

        semiTrans.setAlpha(164);
        ma.setMarkerColor( semiTrans.darker() );

        dva.setMarkerAttributes( ma );

        // While we are at it we also set the text alignment to centered
        // for this special point:
        relativePosition = dva.positivePosition();
        relativePosition.setAlignment( Qt::AlignCenter );
        dva.setPositivePosition( relativePosition );
        diagram->setDataValueAttributes( index, dva);



        // Assign our diagram to the Chart
        m_chart.coordinatePlane()->replaceDiagram(diagram);

        // We want to have a nice gap around the polar diagram,
        // but we also want to have the coord. plane's background cover that area,
        // so we just use some zooming.
        // NOTE: Setting a zoom factor must not be done before
        //       a diagram has been specified and assigned to the coordinate plane!
        plane->setZoomFactorX(0.9);
        plane->setZoomFactorY(0.9);

        QVBoxLayout* l = new QVBoxLayout(this);
        l->addWidget(&m_chart);
        m_chart.setGlobalLeadingTop( 5 );
        m_chart.setGlobalLeadingBottom( 5 );
        setLayout(l);
    }

private:
    Chart m_chart;
    QStandardItemModel m_model;
};

int main( int argc, char** argv ) {
    QApplication app( argc, argv );

    ChartWidget w;
    w.show();

    return app.exec();
}

#include "main.moc"
