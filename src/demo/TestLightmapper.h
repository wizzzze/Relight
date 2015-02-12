//
//  Filename:	TestLightmapper.h
//	Created:	2:6:2012   20:07

#ifndef		__DC_TestLightmapper_H__
#define		__DC_TestLightmapper_H__

/*
=========================================================================================

			HEADERS & DEFS

=========================================================================================
*/

#include	"Test.h"
#include    <Relight.h>
#include    <pthread.h>

#define		MAX_LIGHTMAPS	(16)

/*
=========================================================================================

			CODE

=========================================================================================
*/

class cLightmapper;
class cLightmap;
class cPhotonMap;
class Model_OBJ;
struct sLight;

class BakingProgress : public relight::Progress {
public:

                                BakingProgress( relight::Lightmap* lightmap, unsigned int* textureId )
                                    : m_lightmap( lightmap ), m_textureId( textureId ), m_hasUpdates( false ) {}

    virtual void                notify( int step, int stepCount );

public:

    unsigned int*           m_textureId;
    relight::Lightmap*      m_lightmap;
    bool                    m_hasUpdates;
};

struct WorkerData {
    relight::Scene*                     m_scene;
    relight::IndirectLightSettings      m_indirectLightSettings;
    relight::AmbientOcclusionSettings   m_aoSettings;
    BakingProgress*                     m_progress;
    int                                 m_startIndex;
    int                                 m_step;
};

// ** class cTestLightmapper
class cTestLightmapper : public cTest {
private:

	void			CalculateDirectLight( void );
	void			CalculateIndirectLight( void );

	void			UpdatePixels( const cLightmap *lightmap, float *pixels );

	void			SaveLightmaps( void );
    void            LoadLightmaps( void );

    void            createScene( const char* fileName );

    static void*    worker( void* userData );

public:

	// ** cTest
	virtual void	KeyPressed( int key );
	virtual void	Create( void );
	virtual void	Render( void );

private:

    unsigned int    createTextureFromLightmap( const relight::Lightmap* lightmap ) const;
    void            renderInstance( const relight::Mesh* mesh ) const;

    void            startThread( int index, int threadCount, const relight::IndirectLightSettings& indirectLight, const relight::AmbientOcclusionSettings& ambientOcclusion );

private:

    pthread_t           m_thread;
    WorkerData          m_data;

    BakingProgress*     m_progress;
    relight::Scene*     m_scene;
    relight::Lightmap*  m_diffuse;
    relight::Photonmap* m_photons;
    relight::Mesh*      m_light;
    float               m_rotation;

    unsigned int        m_diffuseGl;
};

#endif	/*	!__DC_TestLightmapper_H__	*/