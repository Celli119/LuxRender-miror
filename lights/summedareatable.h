/**
 * @file    SummedAreaTable.h
 * @author  Aldo Zang <aldo.zang@gmail.com>
 * @author  VISGRAF LAB, IMPA
 * @version 1.5.0
 * @date    01/06/2010
 *
 * @brief   HSM Graphical interface: interface controls
 */
//________________________________________________
#ifndef SUMMED_AREA_TABLE_
#define SUMMED_AREA_TABLE_
#include <stdio.h>
#include <math.h>


//--------------------------------------------------------------------------//

class SummedAreaTable {

	public:
		float *A;
		int width, height;

		SummedAreaTable() {
			A = 0; 
			height = 0; 
			width = 0;
		}
		SummedAreaTable( int w , int h );
		~SummedAreaTable() { if ( A ) delete [] A; }

		bool setSize( int w, int h );
		void clear( float color = 0 );
		void doSum( void );

		int getWidth( void ) { return width; }
		int getHeight( void ) { return height; }

		float &pixel( int x , int y ) { return A[width*y+x]; }
		float &operator()( int x, int y ) { return A[width*y+x]; }
		float sumRectangle( int x0, int y0, int x1, int y1 );
		float pixelOverAverage( int x, int y ) {

			float pixel = sumRectangle(x,y,x,y) * width * height;
			float ave =   sumRectangle(0,0,width-1,height-1);
			return pixel/ave;
		}

		float getMaxValue( void );
		bool writeToPGM( char *fName, char *headerInfo = 0 );
		bool write( void );
};

#endif
