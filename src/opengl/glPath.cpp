/*
 *  glPath.cpp
 *  MonkVG-OpenGL
 *
 *  Created by Micah Pearlman on 8/6/10.
 *  Copyright 2010 MP Engineering. All rights reserved.
 *
 */


#include "glPath.h"
#include "glContext.h"
#include <cassert>

namespace MonkVG {
	
	
	
	void OpenGLPath::clear( VGbitfield caps ) {
		IPath::clear( caps );
		
		_vertices.clear();
		
		// delete vbo buffers
		if ( _strokeVBO != -1 ) {
			glDeleteBuffers( 1, &_strokeVBO );
			_strokeVBO = -1;
		}
		
		if ( _fillVBO != -1 ) {
			glDeleteBuffers( 1, &_fillVBO );
			_fillVBO = -1;
		}
	}
	
	void OpenGLPath::buildFillIfDirty() {
		// only build the fill
		if ( _isFillDirty ) {
			buildFill();
		}
		_isFillDirty = false;
	}
	
	void printMat44( float m[4][4] ) {
		printf("--\n");
		for ( int x = 0; x < 4; x++ ) {
			printf("%f\t%f\t%f\t%f\n", m[x][0], m[x][1], m[x][2], m[x][3]);			
		}
	}
	
	bool OpenGLPath::draw( VGbitfield paintModes ) {
		
		if ( paintModes == 0 ) 
			return false;
		
		CHECK_GL_ERROR;
		
		// get the native OpenGL context
		OpenGLContext& glContext = (MonkVG::OpenGLContext&)IContext::instance();
		
		if( paintModes & VG_FILL_PATH ) {	// build the fill polygons
			buildFillIfDirty();
		}

		if( paintModes & VG_STROKE_PATH && _isStrokeDirty == true ) {
			buildStroke();
			_isStrokeDirty = false;
		}
		
		


		glContext.beginRender();
		
		
//		Matrix33 active = *IContext::instance().getActiveMatrix();
//		// a	b	0
//		// c	d	0
//		// tx	ty	1
//		
//		GLfloat mat44[4][4];
//		for( int x = 0; x < 4; x++ )
//			for( int y = 0; y < 4; y++ )
//				mat44[x][y] = 0;
//		mat44[0][0] = 1.0f;
//		mat44[1][1] = 1.0f;
//		mat44[2][2] = 1.0f;
//		mat44[3][3]	= 1.0f;
//		
//		// rotate (note transposed)
//		mat44[0][0] = active.get( 0, 0 );
//		mat44[0][1] = active.get( 1, 0 );
//		mat44[1][0]	= active.get( 0, 1 );
//		mat44[1][1] = active.get( 1, 1 );
//		
//		// scale
//		mat44[3][0] = active.get( 0, 2 );
//		mat44[3][1] = active.get( 1, 2 );
//		
//		
//		//glMatrixMode( GL_MODELVIEW );
//		glPushMatrix();
//		glLoadMatrixf( &mat44[0][0] );
		
		if( paintModes & VG_FILL_PATH ) {
			
			// draw
//			glDisable(GL_TEXTURE_2D);
//			glDisableClientState( GL_TEXTURE_COORD_ARRAY );
//			glDisableClientState( GL_COLOR_ARRAY );
//			
//			glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);			

			IContext::instance().fill();
			glBindBuffer( GL_ARRAY_BUFFER, _fillVBO );
//			glEnableClientState( GL_VERTEX_ARRAY );
			glVertexPointer( 2, GL_FLOAT, sizeof(float) * 2, 0 );
			glDrawArrays( GL_TRIANGLES, 0, _numberFillVertices );
			glBindBuffer( GL_ARRAY_BUFFER, 0 );
			
		}
		
		if ( paintModes & VG_STROKE_PATH ) {
			// draw
			//glDisable(GL_TEXTURE_2D);
//			glDisableClientState( GL_TEXTURE_COORD_ARRAY );
//			glDisableClientState( GL_COLOR_ARRAY );
			
			IContext::instance().stroke();
			glBindBuffer( GL_ARRAY_BUFFER, _strokeVBO );
//			glEnableClientState( GL_VERTEX_ARRAY );
			glVertexPointer( 2, GL_FLOAT, sizeof(float) * 2, 0 );
			glDrawArrays( GL_TRIANGLE_STRIP, 0, _numberStrokeVertices );
			
			glBindBuffer( GL_ARRAY_BUFFER, 0 );			
			
		}
		
		glContext.endRender();
		//		glPopMatrix();
		
		CHECK_GL_ERROR;
		
		return true;
	}
	
	static inline VGfloat calcCubicBezier1d( VGfloat x0, VGfloat x1, VGfloat x2, VGfloat x3, VGfloat t ) {
		// see openvg 1.0 spec section 8.3.2 Cubic Bezier Curves
		VGfloat oneT = 1.0f - t;
		VGfloat x =		x0 * (oneT * oneT * oneT)
		+	3.0f * x1 * (oneT * oneT) * t
		+	3.0f * x2 * oneT * (t * t)
		+	x3 * (t * t * t);
		return x;	
	}
	
#ifndef M_PI 
#define M_PI 3.14159265358979323846 
#endif
	// Given: Points (x0, y0) and (x1, y1) 
	// Return: TRUE if a solution exists, FALSE otherwise 
	//	Circle centers are written to (cx0, cy0) and (cx1, cy1) 
	static VGboolean findUnitCircles(VGfloat x0, VGfloat y0, VGfloat x1, VGfloat y1,
									 VGfloat *cx0, VGfloat *cy0, VGfloat *cx1, VGfloat *cy1) {								 
		
		// Compute differences and averages
		VGfloat dx = x0 - x1;
		VGfloat dy = y0 - y1;
		VGfloat xm = (x0 + x1)/2;
		VGfloat ym = (y0 + y1)/2; 
		VGfloat dsq, disc, s, sdx, sdy;
		// Solve for intersecting unit circles 
		dsq = dx*dx + dy*dy; 
		if (dsq == 0.0) 
			return VG_FALSE; // Points are coincident 
		disc = 1.0f/dsq - 1.0f/4.0f;
		if (disc < 0.0) 
			return VG_FALSE; // Points are too far apart 
		
		s = sqrt(disc); 
		sdx = s*dx; 
		sdy = s*dy;
		*cx0 = xm + sdy; 
		*cy0 = ym - sdx;
		*cx1 = xm - sdy;
		*cy1 = ym + sdx; 
		
		return VG_TRUE;
	}
	
	//Given: 
	//Return: 
	//Ellipse parameters rh, rv, rot (in degrees), endpoints (x0, y0) and (x1, y1) TRUE if a solution exists, FALSE otherwise. Ellipse centers are written to (cx0, cy0) and (cx1, cy1)
	
	static VGboolean findEllipses(VGfloat rh, VGfloat rv, VGfloat rot,
								  VGfloat x0, VGfloat y0, VGfloat x1, VGfloat y1, 
								  VGfloat *cx0, VGfloat *cy0, VGfloat *cx1, VGfloat *cy1) {
		VGfloat COS, SIN, x0p, y0p, x1p, y1p, pcx0, pcy0, pcx1, pcy1;
		// Convert rotation angle from degrees to radians 
		rot *= M_PI/180.0;
		// Pre-compute rotation matrix entries 
		COS = cos(rot); 
		SIN = sin(rot);
		// Transform (x0, y0) and (x1, y1) into unit space 
		// using (inverse) rotate, followed by (inverse) scale	
		x0p = (x0*COS + y0*SIN)/rh; 
		y0p = (-x0*SIN + y0*COS)/rv; 
		x1p = (x1*COS + y1*SIN)/rh; 
		y1p = (-x1*SIN + y1*COS)/rv;
		if (!findUnitCircles(x0p, y0p, x1p, y1p, &pcx0, &pcy0, &pcx1, &pcy1)) {
			return VG_FALSE;
		}
		// Transform back to original coordinate space 
		// using (forward) scale followed by (forward) rotate 
		pcx0 *= rh; 
		pcy0 *= rv; 
		pcx1 *= rh; 
		pcy1 *= rv;
		*cx0 = pcx0*COS - pcy0*SIN; 
		*cy0 = pcx0*SIN + pcy0*COS; 
		*cx1 = pcx1*COS - pcy1*SIN; 
		*cy1 = pcx1*SIN + pcy1*COS;
		
		return VG_TRUE;	
	}
	
	
	void OpenGLPath::buildFill() {
		
		CHECK_GL_ERROR;
		

		_fillTesseleator = gluNewTess();
		gluTessCallback( _fillTesseleator, GLU_TESS_BEGIN_DATA, (GLvoid (*) ( )) &OpenGLPath::tessBegin );
		gluTessCallback( _fillTesseleator, GLU_TESS_END_DATA, (GLvoid (*) ( )) &OpenGLPath::tessEnd );
		gluTessCallback( _fillTesseleator, GLU_TESS_VERTEX_DATA, (GLvoid (*) ( )) &OpenGLPath::tessVertex );
		gluTessCallback( _fillTesseleator, GLU_TESS_COMBINE_DATA, (GLvoid (*) ( )) &OpenGLPath::tessCombine );
		gluTessCallback( _fillTesseleator, GLU_TESS_ERROR, (GLvoid (*)())&OpenGLPath::tessError );
		// TODO: SET WINDING RULE BASED ON THE OPENVG SETTING!!!
		gluTessProperty( _fillTesseleator, GLU_TESS_WINDING_RULE, GLU_TESS_WINDING_ODD ); 
		gluTessProperty( _fillTesseleator, GLU_TESS_TOLERANCE, 0.5f );
		
		gluTessBeginPolygon( _fillTesseleator, this );
		
		
		vector< VGfloat >::iterator coordsIter = _fcoords->begin();
		int numCoords = 0;
		VGbyte segment = VG_CLOSE_PATH;
		v2_t coords = {0,0};
		v2_t prev = {0,0};
		v2_t closeTo = {0,0}; 
		int num_contours = 0;
		
		for ( vector< VGubyte >::iterator segmentIter = _segments.begin(); segmentIter != _segments.end(); segmentIter++ ) {
			segment = (*segmentIter);
			numCoords = segmentToNumCoordinates( static_cast<VGPathSegment>( segment ) );
			//segment = segment >> 1;
			
			
			//			VG_CLOSE_PATH                               = ( 0 << 1),
			//			VG_MOVE_TO                                  = ( 1 << 1),
			//			VG_LINE_TO                                  = ( 2 << 1),
			//			VG_HLINE_TO                                 = ( 3 << 1),
			//			VG_VLINE_TO                                 = ( 4 << 1),
			//			VG_QUAD_TO                                  = ( 5 << 1),
			//			VG_CUBIC_TO                                 = ( 6 << 1),
			//			VG_SQUAD_TO                                 = ( 7 << 1),
			//			VG_SCUBIC_TO                                = ( 8 << 1),
			//			VG_SCCWARC_TO                               = ( 9 << 1),
			//			VG_SCWARC_TO                                = (10 << 1),
			//			VG_LCCWARC_TO                               = (11 << 1),
			//			VG_LCWARC_TO                                = (12 << 1),
			
			// todo: deal with relative move
			bool isRelative = segment & VG_RELATIVE;
			switch (segment >> 1) {
				case (VG_CLOSE_PATH >> 1):
				{
					GLdouble* c = new GLdouble[3];
					c[0] = closeTo.x;
					c[1] = closeTo.y;
					c[2] = 0;
					// do not think this is necessary for the tesselator						
					//gluTessVertex( _fillTesseleator, c, c );
				} break;
				case (VG_MOVE_TO >> 1):
				{	
					if ( num_contours ) {
						gluTessEndContour( _fillTesseleator );
						num_contours--;
					}
					
					gluTessBeginContour( _fillTesseleator );
					num_contours++;
					closeTo.x = coords.x = *coordsIter; coordsIter++;
					closeTo.y = coords.y = *coordsIter; coordsIter++;
					
					v3_t c( coords );
					_tessVertices.push_back( c );
					gluTessVertex( _fillTesseleator, tessVerticesBackPtr(), tessVerticesBackPtr() );
					
				} break;
				case (VG_LINE_TO >> 1):
				{
					prev = coords;
					coords.x = *coordsIter; coordsIter++;
					coords.y = *coordsIter; coordsIter++;
					if ( isRelative ) {
						coords.x += prev.x;
						coords.y += prev.y;
					}
					
					v3_t c( coords );
					_tessVertices.push_back( c );
					gluTessVertex( _fillTesseleator, tessVerticesBackPtr(), tessVerticesBackPtr() );
				} break;
				case (VG_HLINE_TO >> 1):
				{
					prev = coords;
					coords.x = *coordsIter; coordsIter++;
					if ( isRelative ) {
						coords.x += prev.x;
					}
					
					v3_t c( coords );
					_tessVertices.push_back( c );
					gluTessVertex( _fillTesseleator, tessVerticesBackPtr(), tessVerticesBackPtr() );
				} break;
				case (VG_VLINE_TO >> 1):
				{
					prev = coords;
					coords.y = *coordsIter; coordsIter++;
					if ( isRelative ) {
						coords.y += prev.y;
					}
					
					v3_t c( coords );
					_tessVertices.push_back( c );
					gluTessVertex( _fillTesseleator, tessVerticesBackPtr(), tessVerticesBackPtr() );
				} break;
				case (VG_CUBIC_TO >> 1):
				{
					prev = coords;
					VGfloat cp1x = *coordsIter; coordsIter++;
					VGfloat cp1y = *coordsIter; coordsIter++;
					VGfloat cp2x = *coordsIter; coordsIter++;
					VGfloat cp2y = *coordsIter; coordsIter++;
					VGfloat p3x = *coordsIter; coordsIter++;
					VGfloat p3y = *coordsIter; coordsIter++;
					
					if ( isRelative ) {
						cp1x += prev.x;
						cp1y += prev.y;
						cp2x += prev.x;
						cp2y += prev.y;
						p3x += prev.x;
						p3y += prev.y;
					}
					
					VGfloat increment = 1.0f / 4.0f;
					//printf("\tcubic: ");
					for ( VGfloat t = increment; t < 1.0f + increment; t+=increment ) {
						v3_t c;
						c.x = calcCubicBezier1d( coords.x, cp1x, cp2x, p3x, t );
						c.y = calcCubicBezier1d( coords.y, cp1y, cp2y, p3y, t );
						_tessVertices.push_back( c );
						gluTessVertex( _fillTesseleator, tessVerticesBackPtr(), tessVerticesBackPtr() );
					//	c.print();
					}
					//printf("\n");
					coords.x = p3x;
					coords.y = p3y;
					
				} break;
				case (VG_SCCWARC_TO >> 1):
				case (VG_SCWARC_TO >> 1):
				case (VG_LCCWARC_TO >> 1):
				case (VG_LCWARC_TO >> 1):	
					
				{
					VGfloat rh = *coordsIter; coordsIter++;
					VGfloat rv = *coordsIter; coordsIter++;
					VGfloat rot = *coordsIter; coordsIter++;
					VGfloat cp1x = *coordsIter; coordsIter++;
					VGfloat cp1y = *coordsIter; coordsIter++;
					if ( isRelative ) {
						cp1x += prev.x;
						cp1y += prev.y;
					}
					
					// convert to Center Parameterization (see OpenVG Spec Apendix A)
					VGfloat cx0[2];
					VGfloat cx1[2];
					VGboolean success = findEllipses( rh, rv, rot,
													 coords.x, coords.y, cp1x, cp1y,
													 &cx0[0], &cx0[1], &cx1[0], &cx1[1] );
					
					if ( success ) {
						// see: http://en.wikipedia.org/wiki/Ellipse#Ellipses_in_computer_graphics 
						const int steps = 4;
						VGfloat beta = 0;	// angle. todo
						VGfloat sinbeta = sinf( beta );
						VGfloat cosbeta = cosf( beta );
						
						// calculate the start and end angles
						v2_t center;
						center.x = cx0[0];
						center.y = cx0[1];
						v2_t norm[2];
						norm[0].x = center.x - coords.x;
						norm[0].y = center.y - coords.y;
						VGfloat inverse_len = 1.0f/sqrtf( (norm[0].x * norm[0].x) + (norm[0].y * norm[0].y) );
						norm[0].x *= inverse_len;
						norm[0].y *= inverse_len;
						
						norm[1].x = center.x - cp1x;
						norm[1].y = center.y - cp1y;
						inverse_len = 1.0f/sqrtf( (norm[1].x * norm[1].x) + (norm[1].y * norm[1].y) );
						norm[1].x *= inverse_len;
						norm[1].y *= inverse_len;
						VGfloat startAngle = degrees( acosf( -norm[0].x ) );
						VGfloat endAngle = degrees( acosf( -norm[1].x ) );
						
						VGfloat cross = norm[0].x;
						
						if ( cross >= 0  ) {
							startAngle = 360 - startAngle;
							endAngle = 360 - endAngle;
						}
						if ( startAngle > endAngle ) {
							VGfloat tmp = startAngle;
							startAngle = endAngle;
							endAngle = tmp;
							startAngle = startAngle - 90;
							endAngle = endAngle - 90;
						}
						for ( VGfloat g = startAngle; g < endAngle; g+=360/steps ) {
							v3_t c;
							
							VGfloat alpha = g * (M_PI / 180.0f);
							VGfloat sinalpha = sinf( alpha );
							VGfloat cosalpha = cosf( alpha );
							c.x = cx0[0] + (rh * cosalpha * cosbeta - rv * sinalpha * sinbeta);
							c.y = cx0[1] + (rh * cosalpha * sinbeta + rv * sinalpha * cosbeta);
							_tessVertices.push_back( c );
							gluTessVertex( _fillTesseleator, tessVerticesBackPtr(), tessVerticesBackPtr() );
						}
					}
					
					coords.x = cp1x;
					coords.y = cp1y;
					
				} break;
					
				default:
					printf("unkwown command\n");
					break;
			}
		}	// foreach segment
		
		if ( num_contours ) {
			gluTessEndContour( _fillTesseleator );
			num_contours--;
		}
		
		assert(num_contours == 0);
		
		gluTessEndPolygon( _fillTesseleator );
		
		gluDeleteTess( _fillTesseleator );
		
		
		endOfTesselation();
		
		_fillTesseleator = 0;
		
		CHECK_GL_ERROR;
		
	}
	
	void OpenGLPath::buildFatLineSegment( vector<v2_t>& vertices, const v2_t& p0, const v2_t& p1, const float radius ) {
		
		if ( (p0.x == p1.x) && (p0.y == p1.y ) ) {
			return;
		}
		
		float dx = p1.y - p0.y;
		float dy = p0.x - p1.x;
		const float inv_mag = 1.0f / sqrtf(dx*dx + dy*dy);
		dx = dx * inv_mag;
		dy = dy * inv_mag;
		
		v2_t v0, v1, v2, v3;
		
		v0.x = p0.x + radius * dx;
		v0.y = p0.y + radius * dy;
		vertices.push_back( v0 );
		
		v1.x = p0.x - radius * dx;
		v1.y = p0.y - radius * dy;
		vertices.push_back( v1 );			
		
		
		v2.x = p1.x + radius * dx;
		v2.y = p1.y + radius * dy;
		vertices.push_back( v2 );			
		
		v3.x = p1.x - radius * dx;
		v3.y = p1.y - radius * dy;
		vertices.push_back( v3 );
		
		//		printf("start stroke\n");
		//		printf("p0: ");p0.print();
		//		printf("p1: ");p1.print();
		//		printf("\t"); v0.print();
		//		printf("\t"); v1.print();
		//		printf("\t"); v2.print();
		//		printf("\t"); v3.print();
		//		printf("end stroke\n");
		
	}
	
	void OpenGLPath::buildStroke() {
		
		// get the native OpenGL context
		OpenGLContext& glContext = (MonkVG::OpenGLContext&)IContext::instance();
		
		const VGfloat stroke_width = glContext.getStrokeLineWidth();
		
		vector< VGfloat >::iterator coordsIter = _fcoords->begin();
		int numCoords = 0;
		VGbyte segment = VG_CLOSE_PATH;
		v2_t coords = {0,0};
		v2_t prev = {0,0};
		v2_t closeTo = {0,0}; 
		vector<v2_t> vertices;
		for ( vector< VGubyte >::iterator segmentIter = _segments.begin(); segmentIter != _segments.end(); segmentIter++ ) {
			segment = (*segmentIter);
			numCoords = segmentToNumCoordinates( static_cast<VGPathSegment>( segment ) );
			//segment = segment >> 1;
			
			
			//			VG_CLOSE_PATH                               = ( 0 << 1),
			//			VG_MOVE_TO                                  = ( 1 << 1),
			//			VG_LINE_TO                                  = ( 2 << 1),
			//			VG_HLINE_TO                                 = ( 3 << 1),
			//			VG_VLINE_TO                                 = ( 4 << 1),
			//			VG_QUAD_TO                                  = ( 5 << 1),
			//			VG_CUBIC_TO                                 = ( 6 << 1),
			//			VG_SQUAD_TO                                 = ( 7 << 1),
			//			VG_SCUBIC_TO                                = ( 8 << 1),
			//			VG_SCCWARC_TO                               = ( 9 << 1),
			//			VG_SCWARC_TO                                = (10 << 1),
			//			VG_LCCWARC_TO                               = (11 << 1),
			//			VG_LCWARC_TO                                = (12 << 1),
			
			// todo: deal with relative move
			bool isRelative = segment & VG_RELATIVE;
			switch (segment >> 1) {
				case (VG_CLOSE_PATH >> 1):
				{
					buildFatLineSegment( vertices, coords, closeTo, stroke_width );
				} break;
				case (VG_MOVE_TO >> 1):
				{	
					prev.x = closeTo.x = coords.x = *coordsIter; coordsIter++;
					prev.y = closeTo.y = coords.y = *coordsIter; coordsIter++;
					
				} break;
				case (VG_LINE_TO >> 1):
				{
					prev = coords;
					coords.x = *coordsIter; coordsIter++;
					coords.y = *coordsIter; coordsIter++;
					if ( isRelative ) {
						coords.x += prev.x;
						coords.y += prev.y;
					}
					
					buildFatLineSegment( vertices, prev, coords, stroke_width );
					
					
				} break;
				case (VG_HLINE_TO >> 1):
				{
					prev = coords;
					coords.x = *coordsIter; coordsIter++;
					if ( isRelative ) {
						coords.x += prev.x;
					}
					
					buildFatLineSegment( vertices, prev, coords, stroke_width );
				} break;
				case (VG_VLINE_TO >> 1):
				{
					prev = coords;
					coords.y = *coordsIter; coordsIter++;
					if ( isRelative ) {
						coords.y += prev.y;
					}
					
					buildFatLineSegment( vertices, prev, coords, stroke_width );
					
				} break;
				case (VG_CUBIC_TO >> 1):	// todo
				{
					prev = coords;
					VGfloat cp1x = *coordsIter; coordsIter++;
					VGfloat cp1y = *coordsIter; coordsIter++;
					VGfloat cp2x = *coordsIter; coordsIter++;
					VGfloat cp2y = *coordsIter; coordsIter++;
					VGfloat p3x = *coordsIter; coordsIter++;
					VGfloat p3y = *coordsIter; coordsIter++;
					
					if ( isRelative ) {
						cp1x += prev.x;
						cp1y += prev.y;
						cp2x += prev.x;
						cp2y += prev.y;
						p3x += prev.x;
						p3y += prev.y;
					}
					
					
					VGfloat increment = 1.0f / 4.0f;
					
					for ( VGfloat t = increment; t < 1.0f + increment; t+=increment ) {
						v2_t c;
						c.x = calcCubicBezier1d( coords.x, cp1x, cp2x, p3x, t );
						c.y = calcCubicBezier1d( coords.y, cp1y, cp2y, p3y, t );
						buildFatLineSegment( vertices, prev, c, stroke_width );
						prev = c;
					}
					coords.x = p3x;
					coords.y = p3y;
					
				} break;
				case (VG_SCCWARC_TO >> 1):
				case (VG_SCWARC_TO >> 1):
				case (VG_LCCWARC_TO >> 1):
				case (VG_LCWARC_TO >> 1):	
	
				{
					VGfloat rh = *coordsIter; coordsIter++;
					VGfloat rv = *coordsIter; coordsIter++;
					VGfloat rot = *coordsIter; coordsIter++;
					VGfloat cp1x = *coordsIter; coordsIter++;
					VGfloat cp1y = *coordsIter; coordsIter++;
					if ( isRelative ) {
						cp1x += prev.x;
						cp1y += prev.y;
					}
					
					
					// convert to Center Parameterization (see OpenVG Spec Apendix A)
					VGfloat cx0[2];
					VGfloat cx1[2];
					VGboolean success = findEllipses( rh, rv, rot,
													 coords.x, coords.y, cp1x, cp1y,
													 &cx0[0], &cx0[1], &cx1[0], &cx1[1] );
					
					if ( success ) {
						// see: http://en.wikipedia.org/wiki/Ellipse#Ellipses_in_computer_graphics 
						const int steps = 36;
						VGfloat beta = 0;	// angle. todo
						VGfloat sinbeta = sinf( beta );
						VGfloat cosbeta = cosf( beta );
						
						// calculate the start and end angles
						v2_t center;
						center.x = cx0[0];//(cx0[0] + cx1[0])*0.5f;
						center.y = cx0[1];//(cx0[1] + cx1[1])*0.5f;
						v2_t norm[2];
						norm[0].x = center.x - coords.x;
						norm[0].y = center.y - coords.y;
						VGfloat inverse_len = 1.0f/sqrtf( (norm[0].x * norm[0].x) + (norm[0].y * norm[0].y) );
						norm[0].x *= inverse_len;
						norm[0].y *= inverse_len;
						
						norm[1].x = center.x - cp1x;
						norm[1].y = center.y - cp1y;
						inverse_len = 1.0f/sqrtf( (norm[1].x * norm[1].x) + (norm[1].y * norm[1].y) );
						norm[1].x *= inverse_len;
						norm[1].y *= inverse_len;
						VGfloat startAngle = degrees( acosf( -norm[0].x ) );
						VGfloat endAngle = degrees( acosf( -norm[1].x ) );
						VGfloat cross = norm[0].x;
						if ( cross >= 0 ) {
							startAngle = 360 - startAngle;
							endAngle = 360 - endAngle;
						}
						if ( startAngle > endAngle ) {
							VGfloat tmp = startAngle;
							startAngle = endAngle;
							endAngle = tmp;
							startAngle = startAngle - 90;
							endAngle = endAngle - 90;
						}
						
						
						prev = coords;
						for ( VGfloat g = startAngle; g < endAngle + (360/steps); g+=360/steps ) {
							v2_t c;
							
							VGfloat alpha = g * (M_PI / 180.0f);
							VGfloat sinalpha = sinf( alpha );
							VGfloat cosalpha = cosf( alpha );
							c.x = cx0[0] + (rh * cosalpha * cosbeta - rv * sinalpha * sinbeta);
							c.y = cx0[1] + (rh * cosalpha * sinbeta + rv * sinalpha * cosbeta);
							//printf( "(%f, %f)\n", c[0], c[1] );
							buildFatLineSegment( vertices, prev, c, stroke_width );
							prev = c;
						}
					}
					
					coords.x = cp1x;
					coords.y = cp1y;
					
				} break;
					
				default:
					printf("unkwown command\n");
					break;
			}
		}	// foreach segment
		
		// build the vertex buffer object VBO
		if ( _strokeVBO != -1 ) {
			glDeleteBuffers( 1, &_strokeVBO );
			_strokeVBO = -1;
		}
		
		glGenBuffers( 1, &_strokeVBO );
		glBindBuffer( GL_ARRAY_BUFFER, _strokeVBO );
		glBufferData( GL_ARRAY_BUFFER, vertices.size() * sizeof(float) * 2, &vertices[0], GL_STATIC_DRAW );
		_numberStrokeVertices = vertices.size();
		
		
	}
	
	void OpenGLPath::endOfTesselation() {
		if ( _fillVBO != -1 ) {
			glDeleteBuffers( 1, &_fillVBO );
			_fillVBO = -1;
		}
		
		glGenBuffers( 1, &_fillVBO );
		glBindBuffer( GL_ARRAY_BUFFER, _fillVBO );
		glBufferData( GL_ARRAY_BUFFER, _vertices.size() * sizeof(float), &_vertices[0], GL_STATIC_DRAW );
		_numberFillVertices = _vertices.size()/2;
		_tessVertices.clear();
		_vertices.clear();
	}
	
	static GLdouble startVertex_[2];
	static GLdouble lastVertex_[2];
	static int vertexCount_ = 0;
	
	void OpenGLPath::tessBegin( GLenum type, GLvoid* user ) {
		OpenGLPath* me = (OpenGLPath*)user;
		me->setPrimType( type );
		vertexCount_ = 0;
		
		switch( type )
		{
			case GL_TRIANGLES:
				//printf( "begin(GL_TRIANGLES)\n" );
				break;
			case GL_TRIANGLE_FAN:
				//printf( "begin(GL_TRIANGLE_FAN)\n" );
				break;
			case GL_TRIANGLE_STRIP:
				//printf( "begin(GL_TRIANGLE_STRIP)\n" );
				break;
			case GL_LINE_LOOP:
				//printf( "begin(GL_LINE_LOOP)\n" );
				break;
			default:
				break;
		}
		
	}
	
	
	void OpenGLPath::tessEnd( GLvoid* user ) {
		//		OpenGLPath* me = (OpenGLPath*)user;
		//		me->endOfTesselation();
		
		//		printf("end\n");
	}
	
	
	void OpenGLPath::tessVertex( GLvoid* vertex, GLvoid* user ) {
		OpenGLPath* me = (OpenGLPath*)user;
		GLdouble* v = (GLdouble*)vertex;
		
		if ( me->primType() == GL_TRIANGLE_FAN ) {
			// break up fans and strips into triangles
			switch ( vertexCount_ ) {
				case 0:
					startVertex_[0] = v[0];
					startVertex_[1] = v[1];
					break;
				case 1:
					lastVertex_[0] = v[0];
					lastVertex_[1] = v[1];
					break;
					
				default:
					me->addVertex( startVertex_ );
					me->addVertex( lastVertex_ );
					me->addVertex( v );
					lastVertex_[0] = v[0];
					lastVertex_[1] = v[1];
					break;
			}
		} else if ( me->primType() == GL_TRIANGLES ) {
			me->addVertex( v );
		} else if ( me->primType() == GL_TRIANGLE_STRIP ) {
			switch ( vertexCount_ ) {
				case 0:
					me->addVertex( v );
					break;
				case 1:
					startVertex_[0] = v[0];
					startVertex_[1] = v[1];
					me->addVertex( v );
					break;
				case 2:
					lastVertex_[0] = v[0];
					lastVertex_[1] = v[1];
					me->addVertex( v );
					break;
					
				default:
					me->addVertex( startVertex_ );
					me->addVertex( lastVertex_ );
					me->addVertex( v );
					startVertex_[0] = lastVertex_[0];
					startVertex_[1] = lastVertex_[1];
					lastVertex_[0] = v[0];
					lastVertex_[1] = v[1];
					break;
			}
		}
		vertexCount_++;
		
				//printf("\tvert[%d]: %f, %f, %f\n", vertexCount_, v[0], v[1], v[2] );
	}
	void OpenGLPath::tessCombine( GLdouble coords[3], void *data[4],
								 GLfloat weight[4], void **outData,
								 void *polygonData ) {

		OpenGLPath* me = (OpenGLPath*)polygonData;
		v3_t v;
		v.x = coords[0];
		v.y = coords[1];
		v.z = coords[2];
		me->addTessVertex( v );
		*outData = me->tessVerticesBackPtr();
		
//		GLdouble* vertex = new GLdouble[3];
//		vertex[0] = coords[0];
//		vertex[1] = coords[1];
//		vertex[2] = coords[2];		
//		*outData = vertex;
//		//todo!!!		me->addVertexToDestroy( v );
//printf("combine\n");
		
	}
	
	void OpenGLPath::tessError( GLenum errorCode ) {
		printf("tesselator error: [%d] %s\n", errorCode, gluErrorString( errorCode) );
	}
	
	OpenGLPath::~OpenGLPath() {
		if ( _fillTesseleator ) {
			gluDeleteTess( _fillTesseleator );
			_fillTesseleator = 0;
		}
		
		glDeleteBuffers( 1, &_fillVBO );
	}
	
}