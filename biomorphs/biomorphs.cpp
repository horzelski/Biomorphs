#include "biomorphs.h"
#include "core\random.h"

#include <ctime>

Biomorphs::Biomorphs( void* userData )
	: m_appConfig(*((D3DAppConfig*)userData))
	, m_doScreenshots(false)
{
}

Biomorphs::~Biomorphs()
{
}

void Biomorphs::_resetDNA()
{
	// generate a new random seed
	static int testSeed = 0xffffffff;

	int seed = testSeed == 0xffffffff ? time(NULL) : testSeed;
	printf("Seed: %x\n", seed );
	Random::seed( seed );

	// initialise dna values to a random tree-ish start point
	D3DXVECTOR3 baseColour( Random::getFloat( 0.5f, 1.0f ),
							Random::getFloat( 0.5f, 1.0f ),
							Random::getFloat( 0.5f, 1.0f ) );

	D3DXVECTOR3 colourMod( Random::getFloat( 0.6f, 1.4f ),
							Random::getFloat( 0.6f, 1.4f ),
							Random::getFloat( 0.6f, 1.4f ) );

	m_testDNA = MAKEDNA(	Random::getInt(4, 8), 
							Random::getFloat(10.0f,120.0f), 
							Random::getFloat(0.8f,1.0f), 
							Random::getFloat( 0.8f, 1.2f ), 
							Random::getFloat( 0.7f, 1.3f ),
							baseColour,
							colourMod);

	m_generation = 0;
}

void Biomorphs::_render()
{
	// render the current generation to a texture
	float aspect = (float)m_appConfig.m_windowWidth / (float)m_appConfig.m_windowHeight;
	D3DXVECTOR4 posScale( 0.0f, 0.0f, 0.1f, 0.1f );
	m_morphRenderer.StartRendering();
	m_morphRenderer.DrawBiomorph( m_testDNA );
	m_morphRenderer.EndRendering( posScale );

	// switch back to rendering to back buffer
	Rendertarget& backBuffer = m_device.GetBackBuffer();
	DepthStencilBuffer& depthBuffer = m_device.GetDepthStencilBuffer();
	m_device.SetRenderTargets( &backBuffer, &depthBuffer );

	// Set the viewport
	Viewport vp;
	vp.topLeft = Vector2(0,0);
	vp.depthRange = Vector2f(0.0f,1.0f);
	vp.dimensions = Vector2(m_appConfig.m_windowWidth, m_appConfig.m_windowHeight);
	m_device.SetViewport( vp );

	// Clear buffers
	static float clearColour[4] = {0.0f, 0.1f, 0.25f, 1.0f};
	m_device.ClearTarget( backBuffer, clearColour );
	m_device.ClearTarget( depthBuffer, 1.0f, 0 );

	// draw the biomorph as a sprite

	Texture2D& morphTexture = m_morphRenderer.CopyOutputTexture(m_spriteRender.GetTexture());
	m_spriteRender.GetTexture() = morphTexture;
		 
	m_spriteRender.RemoveSprites();
	m_spriteRender.AddSprite( 0, D3DXVECTOR2(-0.7f,-0.7f), D3DXVECTOR2(1.4f,1.4f) );
	float scale = 0.6f;
	m_spriteRender.Draw( m_device, D3DXVECTOR2(0.0f,0.0f), D3DXVECTOR2(scale,scale*aspect), "Render" );

	//now render the bloom from the backbuffer
	m_bloom.Render();

	char textOut[256] = {'\0'};
	Font::DrawParameters dp;
	const float textColour[] = {1.0f,1.0f,1.0f,1.0f};
	Vector2 textPos( 16, m_appConfig.m_windowHeight - 64 );
	memcpy( dp.mColour, textColour, sizeof(textColour) );

	sprintf_s(textOut, "Generation: %d", m_generation);
	dp.mJustification = Font::DRAW_LEFT;
	m_device.DrawText( textOut, m_font, dp, textPos );

	textPos.y() = textPos.y() + 18;
	sprintf_s(textOut, "DNA: %x%x%x%x", m_testDNA.mFullSequenceHigh0, m_testDNA.mFullSequenceLow0
										  , m_testDNA.mFullSequenceHigh1, m_testDNA.mFullSequenceLow1);
	m_device.DrawText( textOut, m_font, dp, textPos );

	textPos.y() = textPos.y() + 18;
	sprintf_s(textOut, "%d Vertices", m_morphRenderer.GetVertexCount());
	m_device.DrawText( textOut, m_font, dp, textPos );

	m_device.PresentBackbuffer();

	// build a file name from the index + DNA identifier
	if( m_doScreenshots )
	{
		char dnaFilename[512] = {'\0'};
		if( m_generation < 10 )
		{
			sprintf_s( dnaFilename, "screenshots//00%u", m_generation);
		}
		else if( m_generation < 100 )
		{
				sprintf_s( dnaFilename, "screenshots//0%u", m_generation);
		}
		else
		{
			sprintf_s( dnaFilename, "screenshots//%u", m_generation);
		}
		m_screenshotHelper.TakeScreenshot(dnaFilename);
	}
}

bool Biomorphs::_initialise()
{
	// load the sprite shader
	Effect::Parameters ep("shaders//textured_sprite.fx");
	m_spriteShader = m_device.CreateEffect(ep);

	// create the morph renderer
	MorphRender::Parameters p;
	p.mTextureHeight = 512;
	p.mTextureWidth = 512;
	if( !m_morphRenderer.Initialise( &m_device, p ) )
	{
		return false;
	}

	// Create a sprite renderer
	SpriteRender::Parameters sp;
	sp.mMaxSprites = 1024 * 8;
	sp.shader = m_spriteShader;
	sp.texture = m_morphRenderer.CopyOutputTexture(sp.texture);
	m_spriteRender.Create( m_device, sp );

	m_screenshotHelper.Initialise( &m_device );

	BloomRender::Parameters bp;
	bp.mWidth = m_appConfig.m_windowWidth;
	bp.mHeight = m_appConfig.m_windowHeight;
	m_bloom.Create( &m_device, bp );

	_resetDNA();

	return true;
}

bool Biomorphs::_update(Timer& timer)
{
	static float evolutionTime = 0.02f; 
	static float timeSinceEvolution = evolutionTime;

	if( m_inputModule->keyPressed( VK_SPACE ) )
	{
		_resetDNA();
	}

	timeSinceEvolution -= timer.getDelta();
	if( timeSinceEvolution < 0.0f )
	{
		timeSinceEvolution = evolutionTime;

		int gene = Random::getInt(0,10);
		int direction = Random::getInt(0,100);
		int dir = direction > 50 ? 1 : -1;
		switch(gene)
		{
		case 0:
			{
				int bd = m_testDNA.mBranchDepth + dir;
				bd = (bd < 2) ? 2 : bd;
				bd = (bd > 10) ? 10 : bd;
				m_testDNA.mBranchDepth = bd;
			}
			break;
		case 1:
			{
				int bd = m_testDNA.mBranchInitialAngle + dir;
				bd = (bd < 0) ? 0 : bd;
				bd = (bd > 127) ? 127 : bd;
				m_testDNA.mBranchInitialAngle = bd;
			}
			break;
		case 2:
			{
				int bd = m_testDNA.mBranchInitialLength + dir;
				bd = (bd < 0) ? 0 : bd;
				bd = (bd > 63) ? 63 : bd;
				m_testDNA.mBranchInitialLength = bd;
			}
			break;
		case 3:
			{
				int bd = m_testDNA.mBranchLengthModifier + dir;
				bd = (bd < 0) ? 0 : bd;
				bd = (bd > 255) ? 255 : bd;
				m_testDNA.mBranchLengthModifier = bd;
			}
			break;
		case 4:
			{
				int bd = m_testDNA.mBranchAngleModifier + dir;
				bd = (bd < 0) ? 0 : bd;
				bd = (bd > 255) ? 255 : bd;
				m_testDNA.mBranchAngleModifier = bd;
			}
			break;
		case 5:
			{
				int bd = m_testDNA.mBaseColourRed + dir;
				bd = bd < 0 ? 0 : bd;
				bd = bd > 31 ? 31 : bd;
				m_testDNA.mBaseColourRed = bd;
			}
		case 6:
			{
				int bd = m_testDNA.mBaseColourGreen + dir;
				bd = bd < 0 ? 0 : bd;
				bd = bd > 31 ? 31 : bd;
				m_testDNA.mBaseColourGreen = bd;
			}
		case 7:
			{
				int bd = m_testDNA.mBaseColourBlue + dir;
				bd = bd < 0 ? 0 : bd;
				bd = bd > 31 ? 31 : bd;
				m_testDNA.mBaseColourBlue = bd;
			}
		case 8:
			{
				int bd = m_testDNA.mBranchRedModifier + dir;
				bd = bd < 0 ? 0 : bd;
				bd = bd > 255 ? 255 : bd;
				m_testDNA.mBranchRedModifier = bd;
			}
		case 9:
			{
				int bd = m_testDNA.mBranchGreenModifier + dir;
				bd = bd < 0 ? 0 : bd;
				bd = bd > 255 ? 255 : bd;
				m_testDNA.mBranchGreenModifier = bd;
			}
		case 10:
			{
				int bd = m_testDNA.mBranchBlueModifier + dir;
				bd = bd < 0 ? 0 : bd;
				bd = bd > 255 ? 255 : bd;
				m_testDNA.mBranchBlueModifier = bd;
			}
		}

		m_generation++;
	}

	return true;
}

bool Biomorphs::update( Timer& timer )
{
	_update(timer);
	_render();

	return true;
}

bool Biomorphs::shutdown()
{
	m_bloom.Release();

	m_device.Release(m_spriteRender.GetTexture());

	m_device.Release( m_font );

	m_screenshotHelper.Release();
	m_morphRenderer.Release();

	m_device.Shutdown();
	return true;
}

bool Biomorphs::connect( IModuleConnector& connector )
{
	m_inputModule = (InputModule*)connector.getModule( "Input" );
	return true;
}

bool Biomorphs::startup()
{
	if( !initDevice() )
		return false;

	// Create a font
	Font::Parameters fontParams;
	fontParams.mSize = 16;
	fontParams.mWeight = 400;
	strcpy_s(fontParams.mTypeface, "Arial");
	m_font = m_device.CreateFont( fontParams );

	return _initialise();
}

bool Biomorphs::initDevice()
{
	// Init the device
	Device::InitParameters params;
	params.enableDebugD3d = true;
	params.msaaCount = 1;
	params.msaaQuality = 0;
	params.windowHeight = m_appConfig.m_windowHeight;
	params.windowWidth = m_appConfig.m_windowWidth;
	if(!m_device.Initialise(params))
	{
		return false;
	}

	return true;
}