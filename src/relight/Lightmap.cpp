/**************************************************************************

 The MIT License (MIT)

 Copyright (c) 2015 Dmitry Sovetov

 https://github.com/dmsovetov

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:
 The above copyright notice and this permission notice shall be included in all
 copies or substantial portions of the Software.
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 SOFTWARE.

 **************************************************************************/

#include "BuildCheck.h"

#include "Lightmap.h"
#include "scene/Mesh.h"

namespace relight {

// ------------------------------------------------ Lightmap ------------------------------------------------ //

// ** Lightmap::Lightmap
Lightmap::Lightmap( int width, int height ) : m_width( width ), m_height( height )
{
    m_lumels.resize( width * height );
}

// ** Lightmap::width
int Lightmap::width( void ) const
{
    return m_width;
}

// ** Lightmap::height
int Lightmap::height( void ) const
{
    return m_height;
}

// ** Lightmap::lumels
Lumel* Lightmap::lumels( void )
{
    return &m_lumels[0];
}

// ** Lightmap::lumel
Lumel& Lightmap::lumel( const Uv& uv )
{
	int x = static_cast<int>( uv.x * (m_width  - 1) );
	int y = static_cast<int>( uv.y * (m_height - 1) );

    return lumel( x, y );
}

// ** Lightmap::lumel
const Lumel& Lightmap::lumel( const Uv& uv ) const
{
	int x = static_cast<int>( uv.x * (m_width  - 1) );
	int y = static_cast<int>( uv.y * (m_height - 1) );

    return lumel( x, y );
}

// ** Lightmap::lumel
Lumel& Lightmap::lumel( int x, int y )
{
    DC_BREAK_IF( x < 0 || x >= m_width || y < 0 || y >= m_height );
    return m_lumels[y * m_width + x];
}

// ** Lightmap::lumel
const Lumel& Lightmap::lumel( int x, int y ) const
{
    DC_BREAK_IF( x < 0 || x >= m_width || y < 0 || y >= m_height );
    return m_lumels[y * m_width + x];
}

// ** Lightmap::addMesh
RelightStatus Lightmap::addMesh( const Mesh* mesh )
{
    if( mesh->lightmap() ) {
        return RelightInvalidCall;
    }

    // ** Attach this lightmap to an instance
    const_cast<Mesh*>( mesh )->setLightmap( this );

    // ** Initialize lumels
    initializeLumels( mesh );

    return RelightSuccess;
}

// ** Lightmap::initializeLumels
void Lightmap::initializeLumels( const Mesh* mesh )
{
    // ** For each face in a sub mesh
    for( int i = 0, n = mesh->faceCount(); i < n; i++ ) {
        initializeLumels( mesh->face( i ) );
    }
}

// ** Lightmap::rect
void Lightmap::rect( const Rect& uv, int& x1, int& y1, int& x2, int& y2 ) const
{
	const Vec2& min = uv.min();
	const Vec2& max = uv.max();

	DC_BREAK_IF( min.x < 0.0f || min.y < 0.0f );
	DC_BREAK_IF( max.x > 1.0f || max.y > 1.0f );

    x1 = static_cast<int>( min.x * m_width );
    x2 = static_cast<int>( max.x * m_width );
    y1 = static_cast<int>( min.y * m_height );
    y2 = static_cast<int>( max.y * m_height );
}

// ** Lightmap::initializeLumels
void Lightmap::initializeLumels( const Face& face )
{
	// ** Get the UV bounding rect
	s32 uStart, uEnd, vStart, vEnd;
	rect( face.uvRect(), uStart, vStart, uEnd, vEnd );

    // ** Initialize lumels
    for( s32 v = vStart; v <= min2( vEnd, m_height - 1 ); v++ ) {
        for( s32 u = uStart; u <= min2( uEnd, m_width - 1 ); u++ ) {
            Lumel& lumel = m_lumels[m_width * v + u];

            Uv uv( (u + 0.5f) / float( m_width ), (v + 0.5f) / float( m_height ) );
            Barycentric barycentric;

            if( !face.isUvInside( uv, barycentric, Vertex::Lightmap ) ) {
                continue;
            }

            initializeLumel( lumel, face, barycentric );
        }
    }
}

// ** Lightmap::initializeLumel
void Lightmap::initializeLumel( Lumel& lumel, const Face& face, const Uv& barycentric )
{
    lumel.m_faceIdx     = face.faceIdx();
    lumel.m_position    = face.positionAt( barycentric );
    lumel.m_normal      = face.normalAt( barycentric );
    lumel.m_color       = Rgb( 0, 0, 0 );
    lumel.m_flags       = lumel.m_flags | Lumel::Valid;
}

// ** Lightmap::expand
void Lightmap::expand( void )
{
    for( int y = 1; y < m_height - 1; y++ ) {
        for( int x = 1; x < m_width - 1; x++ ) {
            Lumel& current = m_lumels[y * m_width + x];
            if( !current ) {
                continue;
            }

            fillInvalidAt( x - 1, y - 1, current.m_color );
            fillInvalidAt( x - 1, y + 1, current.m_color );
            fillInvalidAt( x + 1, y - 1, current.m_color );
            fillInvalidAt( x + 1, y + 1, current.m_color );
        }
    }
}

// ** Lightmap::fillInvalidAt
void Lightmap::fillInvalidAt( int x, int y, const Rgb& color )
{
    Lumel& l = lumel( x, y );

    if( l ) {
        return;
    }

    l.m_color = color;
}

// ** Lightmap::blur
void Lightmap::blur( void )
{
    for( int y = 1; y < m_height - 1; y++ ) {
        for( int x = 1; x < m_width - 1; x++ ) {
            Rgb color( 0, 0, 0 );
            int count = 0;

            for( int j = y - 1; j <= y + 1; j++ ) {
                for( int i = x - 1; i <= x + 1; i++ ) {
                    const Lumel& lumel = m_lumels[j * m_width + i];

                    if( lumel  ) {
                        color += lumel.m_color;
                        count++;
                    }
                }
            }
            
            if( count ) {
                m_lumels[y * m_width + x].m_color = color * (1.0f / count);
            }
        }
    }
}
    
// ** Lightmap::save
bool Lightmap::save( const String& fileName, StorageFormat format ) const
{
    FILE	*file;

    file = fopen( fileName.c_str(), "wb" );
    if( !file ) {
        return false;
    }

    if( format == RawHdr ) {
        fwrite( &m_width,  1, sizeof( int ), file );
        fwrite( &m_height, 1, sizeof( int ), file );

        float* pixels = toHdr();
        fwrite( pixels, sizeof( float ), m_width * m_height * 3, file );
        delete[]pixels;
    }
    else {
        unsigned char* pixels   = NULL;
        unsigned int   channels = 0;

        switch( format ) {
        case TgaDoubleLdr:  pixels   = toDoubleLdr();
                            channels = 3;
                            break;

        case TgaRgbm:       pixels = toRgbmLdr();
                            channels = 4;
                            break;
        }

        unsigned char tga_header_a[12]   = { 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
        unsigned char tga_info_header[6] = { 0, 0, 0, 0, 0, 0 };

        fwrite( tga_header_a, 1, sizeof( tga_header_a ), file );

        tga_info_header[0] = m_width  % 256;
        tga_info_header[1] = m_width  / 256;
        tga_info_header[2] = m_height % 256;
        tga_info_header[3] = m_height / 256;
        tga_info_header[4] = channels * 8;
        tga_info_header[5] = 0;

        fwrite( tga_info_header, 1, sizeof( tga_info_header ), file );

        for( int i = 0; i < m_width * m_height; i++ ) {
            unsigned char* pixel = &pixels[i * channels];

            fwrite( &pixel[2], 1, 1, file );
            fwrite( &pixel[1], 1, 1, file );
            fwrite( &pixel[0], 1, 1, file );

            if( channels == 4 ) {
                fwrite( &pixel[3], 1, 1, file );
            }
        }

        delete[]pixels;
    }

    fclose( file );

    return true;
}

// ** Lightmap::toRgbmLdr
unsigned char* Lightmap::toRgbmLdr( void ) const
{
    unsigned char* pixels = new unsigned char[m_width * m_height * 4];
    const int      stride = m_width * 4;

    for( int y = 0; y < m_height; y++ ) {
        for( int x = 0; x < m_width; x++ ) {
            const Lumel&   lumel   = m_lumels[y * m_width + x];
            unsigned char* pixel   = &pixels[y * stride + x * 3];
            RgbmLdr        rgbm    = lumel.m_color.rgbm();

            pixel[0] = rgbm.r;
            pixel[1] = rgbm.g;
            pixel[2] = rgbm.b;
            pixel[3] = rgbm.m;
        }
    }

    return pixels;
}

// ** Lightmap::toDoubleLdr
unsigned char* Lightmap::toDoubleLdr( void ) const
{
    unsigned char* pixels = new unsigned char[m_width * m_height * 3];
    const int      stride = m_width * 3;

    for( int y = 0; y < m_height; y++ ) {
        for( int x = 0; x < m_width; x++ ) {
            const Lumel&    lumel   = m_lumels[y * m_width + x];
            unsigned char*  pixel   = &pixels[y * stride + x * 3];
            DoubleLdr       dldr    = lumel.m_color.doubleLdr();

            pixel[0] = dldr.r;
            pixel[1] = dldr.g;
            pixel[2] = dldr.b;
        }
    }

    return pixels;
}

// ** Lightmap::toHdr
float* Lightmap::toHdr( void ) const
{
    float*    pixels = new float[m_width * m_height * 3];
    const int stride = m_width * 3;

    for( int y = 0; y < m_height; y++ ) {
        for( int x = 0; x < m_width; x++ ) {
            const Lumel& lumel = m_lumels[y * m_width + x];
            float*       pixel = &pixels[y * stride + x * 3];

            pixel[0] = lumel.m_color.r;
            pixel[1] = lumel.m_color.g;
            pixel[2] = lumel.m_color.b;
        }
    }
    
    return pixels;
}

// ------------------------------------------------ Photonmap ------------------------------------------------ //

// ** Photonmap::Photonmap
Photonmap::Photonmap( int width, int height ) : Lightmap( width, height )
{

}

// ** Photonmap::addMesh
RelightStatus Photonmap::addMesh( const Mesh* mesh, bool copyVertexColor )
{
    if( mesh->photonmap() ) {
        return RelightInvalidCall;
    }

    // ** Attach this lightmap to an instance
    const_cast<Mesh*>( mesh )->setPhotonmap( this );

    // ** Initialize lumels
    initializeLumels( mesh );

    return RelightSuccess;
}

// ** Photonmap::gather
void Photonmap::gather( int radius )
{
    for( int y = 0; y < m_height; y++ ) {
        for( int x = 0; x < m_height; x++ ) {
            lumel( x, y ).m_gathered = gather( x, y, radius );
        }
    }
}

// ** Photonmap::gather
Rgb Photonmap::gather( int x, int y, int radius ) const
{
    Rgb color;
    int photons = 0;

    for( int j = y - radius; j <= y + radius; j++ ) {
        for( int i = x - radius; i <= x + radius; i++ ) {
            if( i < 0 || j < 0 || i >= m_width || j >= m_height ) {
                continue;
            }

            const Lumel& lumel = m_lumels[j * m_width + i];
            float distance = sqrtf( static_cast<float>( (x - i) * (x - i) + (y - j) * (y - j) ) );
            if( distance > radius ) {
                continue;
            }

            color   += lumel.m_color;
            photons += lumel.m_photons;
        }
    }

    if( photons == 0 ) {
        return Rgb( 0.0f, 0.0f, 0.0f );
    }

    return color / static_cast<float>( photons );
}

// ------------------------------------------------ Radiancemap ------------------------------------------------ //

// ** Radiancemap::Radiancemap
Radiancemap::Radiancemap( int width, int height ) : Lightmap( width, height )
{

}

// ** Radiancemap::addMesh
RelightStatus Radiancemap::addMesh( const Mesh* mesh, bool copyVertexColor )
{
    if( mesh->radiancemap() ) {
        return RelightInvalidCall;
    }

    // ** Attach this lightmap to an instance
    const_cast<Mesh*>( mesh )->setRadiancemap( this );

    // ** Initialize lumels
    initializeLumels( mesh );

    return RelightSuccess;
}

} // namespace relight
