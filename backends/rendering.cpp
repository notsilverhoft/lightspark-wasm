/**************************************************************************
    Lightspark, a free flash player implementation

    Copyright (C) 2009,2010  Alessandro Pignotti (a.pignotti@sssup.it)

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
**************************************************************************/

#include "scripting/abc.h"
#include "parsing/textfile.h"
#include "rendering.h"
#include "compat.h"
#include <sstream>
//#include "swf.h"

#include <SDL.h>

#include <GL/glew.h>
#ifndef WIN32
#include <GL/glx.h>
#include <fontconfig/fontconfig.h>
#endif

using namespace lightspark;
using namespace std;
extern TLSDATA lightspark::SystemState* sys DLL_PUBLIC;
extern TLSDATA RenderThread* rt;

void RenderThread::wait()
{
	if(terminated)
		return;
	terminated=true;
	//Signal potentially blocking semaphore
	sem_post(&event);
	int ret=pthread_join(t,NULL);
	assert_and_throw(ret==0);
}

RenderThread::RenderThread(SystemState* s,ENGINE e,void* params):m_sys(s),terminated(false),currentPixelBuffer(0),currentPixelBufferOffset(0),
	pixelBufferWidth(0),pixelBufferHeight(0),prevUploadJob(NULL),mutexLargeTexture("Large texture"),largeTextureId(0),largeTextureSize(0),
	largeTextureBitmap(NULL),renderNeeded(false),uploadNeeded(false),inputNeeded(false),inputDisabled(false),resizeNeeded(false),newWidth(0),
	newHeight(0),scaleX(1),scaleY(1),offsetX(0),offsetY(0),interactive_buffer(NULL),tempBufferAcquired(false),frameCount(0),secsCount(0),
	mutexUploadJobs("Upload jobs"),initialized(0),dataTex(false),tempTex(false),inputTex(false),hasNPOTTextures(false),selectedDebug(NULL),
	currentId(0),materialOverride(false)
{
	LOG(LOG_NO_INFO,_("RenderThread this=") << this);
	m_sys=s;
	sem_init(&event,0,0);
	sem_init(&inputDone,0,0);

#ifdef WIN32
	fontPath = "TimesNewRoman.ttf";
#else
	FcPattern *pat, *match;
	FcResult result = FcResultMatch;
	char *font = NULL;

	pat = FcPatternCreate();
	FcPatternAddString(pat, FC_FAMILY, (const FcChar8 *)"Serif");
	FcConfigSubstitute(NULL, pat, FcMatchPattern);
	FcDefaultSubstitute(pat);
	match = FcFontMatch(NULL, pat, &result);
	FcPatternDestroy(pat);

	if (result != FcResultMatch)
	{
		LOG(LOG_ERROR,_("Unable to find suitable Serif font"));
		throw RunTimeException("Unable to find Serif font");
	}

	FcPatternGetString(match, FC_FILE, 0, (FcChar8 **) &font);
	fontPath = font;
	FcPatternDestroy(match);
	LOG(LOG_NO_INFO, _("Font File is ") << fontPath);
#endif

	if(e==SDL)
		pthread_create(&t,NULL,(thread_worker)sdl_worker,this);
#ifdef COMPILE_PLUGIN
	else if(e==GTKPLUG)
	{
		npapi_params=(NPAPI_params*)params;
		pthread_create(&t,NULL,(thread_worker)gtkplug_worker,this);
	}
#endif
	time_s = compat_get_current_time_ms();
}

RenderThread::~RenderThread()
{
	wait();
	sem_destroy(&event);
	sem_destroy(&inputDone);
	delete[] interactive_buffer;
	LOG(LOG_NO_INFO,_("~RenderThread this=") << this);
}

void RenderThread::requestInput()
{
	inputNeeded=true;
	sem_post(&event);
	sem_wait(&inputDone);
}

void RenderThread::glAcquireTempBuffer(number_t xmin, number_t xmax, number_t ymin, number_t ymax)
{
	assert(tempBufferAcquired==false);
	tempBufferAcquired=true;

	glBindFramebuffer(GL_FRAMEBUFFER, fboId);
	glDrawBuffer(GL_COLOR_ATTACHMENT0);
	materialOverride=false;
	
	glColor4f(0,0,0,0); //No output is fairly ok to clear
	glBegin(GL_QUADS);
		glVertex2f(xmin,ymin);
		glVertex2f(xmax,ymin);
		glVertex2f(xmax,ymax);
		glVertex2f(xmin,ymax);
	glEnd();
}

void RenderThread::glBlitTempBuffer(number_t xmin, number_t xmax, number_t ymin, number_t ymax)
{
	assert(tempBufferAcquired==true);
	tempBufferAcquired=false;

	//Use the blittler program to blit only the used buffer
	glUseProgram(blitter_program);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glDrawBuffer(GL_BACK);

	rt->tempTex.bind();
	glBegin(GL_QUADS);
		glVertex2f(xmin,ymin);
		glVertex2f(xmax,ymin);
		glVertex2f(xmax,ymax);
		glVertex2f(xmin,ymax);
	glEnd();
	glUseProgram(gpu_program);
}

#ifdef COMPILE_PLUGIN
void* RenderThread::gtkplug_worker(RenderThread* th)
{
	sys=th->m_sys;
	rt=th;
	NPAPI_params* p=th->npapi_params;
	SemaphoreLighter lighter(th->initialized);

	th->windowWidth=p->width;
	th->windowHeight=p->height;
	
	Display* d=XOpenDisplay(NULL);

	int a,b;
	Bool glx_present=glXQueryVersion(d,&a,&b);
	if(!glx_present)
	{
		LOG(LOG_ERROR,_("glX not present"));
		return NULL;
	}
	int attrib[10]={GLX_BUFFER_SIZE,24,GLX_DOUBLEBUFFER,True,None};
	GLXFBConfig* fb=glXChooseFBConfig(d, 0, attrib, &a);
	if(!fb)
	{
		attrib[2]=None;
		fb=glXChooseFBConfig(d, 0, attrib, &a);
		LOG(LOG_ERROR,_("Falling back to no double buffering"));
	}
	if(!fb)
	{
		LOG(LOG_ERROR,_("Could not find any GLX configuration"));
		::abort();
	}
	int i;
	for(i=0;i<a;i++)
	{
		int id;
		glXGetFBConfigAttrib(d,fb[i],GLX_VISUAL_ID,&id);
		if(id==(int)p->visual)
			break;
	}
	if(i==a)
	{
		//No suitable id found
		LOG(LOG_ERROR,_("No suitable graphics configuration available"));
		return NULL;
	}
	th->mFBConfig=fb[i];
	cout << "Chosen config " << hex << fb[i] << dec << endl;
	XFree(fb);

	th->mContext = glXCreateNewContext(d,th->mFBConfig,GLX_RGBA_TYPE ,NULL,1);
	GLXWindow glxWin=p->window;
	glXMakeCurrent(d, glxWin,th->mContext);
	if(!glXIsDirect(d,th->mContext))
		cout << "Indirect!!" << endl;

	th->commonGLInit(th->windowWidth, th->windowHeight);
	th->commonGLResize(th->windowWidth, th->windowHeight);
	lighter.light();
	
	ThreadProfile* profile=sys->allocateProfiler(RGB(200,0,0));
	profile->setTag("Render");
	FTTextureFont font(th->fontPath.c_str());
	if(font.Error())
	{
		LOG(LOG_ERROR,_("Unable to load serif font"));
		throw RunTimeException("Unable to load font");
	}
	
	font.FaceSize(12);

	glEnable(GL_TEXTURE_2D);
	try
	{
		while(1)
		{
			sem_wait(&th->event);
			if(th->m_sys->isShuttingDown() || th->terminated)
				break;
			Chronometer chronometer;
			
			if(th->resizeNeeded)
			{
				if(th->windowWidth!=th->newWidth ||
					th->windowHeight!=th->newHeight)
				{
					th->windowWidth=th->newWidth;
					th->windowHeight=th->newHeight;
					LOG(LOG_ERROR,_("Window resize not supported in plugin"));
				}
				th->newWidth=0;
				th->newHeight=0;
				th->resizeNeeded=false;
				th->commonGLResize(th->windowWidth, th->windowHeight);
				continue;
			}

			if(th->inputNeeded)
			{
				th->inputTex.bind();
				glGetTexImage(GL_TEXTURE_2D,0,GL_BGRA,GL_UNSIGNED_BYTE,th->interactive_buffer);
				th->inputNeeded=false;
				sem_post(&th->inputDone);
			}

			if(th->prevUploadJob)
			{
				ITextureUploadable* u=th->prevUploadJob;
				glBindBuffer(GL_PIXEL_UNPACK_BUFFER, th->pixelBuffers[th->currentPixelBuffer]);
				//Copy content of the pbo to the texture, 0 is the offset in the pbo
				uint32_t w,h;
				u->sizeNeeded(w,h);
				th->loadChunkBGRA(u->getTexture(), w, h, (uint8_t*)th->currentPixelBufferOffset);
				glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
				u->uploadFence();
				th->prevUploadJob=NULL;
			}

			if(th->uploadNeeded)
			{
				ITextureUploadable* u=th->getUploadJob();
				if(u)
				{
					uint32_t w,h;
					u->sizeNeeded(w,h);
					if(w>th->pixelBufferWidth || h>th->pixelBufferHeight)
						th->resizePixelBuffers(w,h);
					//Increment and wrap current buffer index
					unsigned int nextBuffer = (th->currentPixelBuffer + 1)%2;

					glBindBuffer(GL_PIXEL_UNPACK_BUFFER, th->pixelBuffers[nextBuffer]);
					uint8_t* buf=(uint8_t*)glMapBuffer(GL_PIXEL_UNPACK_BUFFER,GL_WRITE_ONLY);
					uint8_t* alignedBuf=(uint8_t*)(uintptr_t((buf+15))&(~0xfL));

					u->upload(alignedBuf, w, h);

					glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
					glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

					th->currentPixelBufferOffset=alignedBuf-buf;
					th->currentPixelBuffer=nextBuffer;
					
					th->prevUploadJob=u;
					continue;
				}
				else if(!th->renderNeeded)
					continue;
			}

			assert(th->renderNeeded);
			if(th->m_sys->isOnError())
			{
				glLoadIdentity();
				glScalef(1.0f/th->scaleX,-1.0f/th->scaleY,1);
				glTranslatef(-th->offsetX,(th->offsetY+th->windowHeight)*(-1.0f),0);
				glUseProgram(0);

				glBindFramebuffer(GL_FRAMEBUFFER, 0);
				glDrawBuffer(GL_BACK);
				glLoadIdentity();

				glClearColor(0,0,0,1);
				glClear(GL_COLOR_BUFFER_BIT);
				glColor3f(0.8,0.8,0.8);
					    
				font.Render("We're sorry, Lightspark encountered a yet unsupported Flash file",
					    -1,FTPoint(0,th->windowHeight/2));

				stringstream errorMsg;
				errorMsg << "SWF file: " << th->m_sys->getOrigin().getParsedURL();
				font.Render(errorMsg.str().c_str(),
					    -1,FTPoint(0,th->windowHeight/2-20));
					    
				errorMsg.str("");
				errorMsg << "Cause: " << th->m_sys->errorCause;
				font.Render(errorMsg.str().c_str(),
					    -1,FTPoint(0,th->windowHeight/2-40));
				
				glFlush();
				glXSwapBuffers(d,glxWin);
			}
			else
			{
				glXSwapBuffers(d,glxWin);
				th->coreRendering(font, false);
				//Call glFlush to offload work on the GPU
				glFlush();
			}
			profile->accountTime(chronometer.checkpoint());
			th->renderNeeded=false;
		}
	}
	catch(LightsparkException& e)
	{
		LOG(LOG_ERROR,_("Exception in RenderThread ") << e.what());
		sys->setError(e.cause);
	}
	glDisable(GL_TEXTURE_2D);
	th->commonGLDeinit();
	glXMakeCurrent(d,None,NULL);
	glXDestroyContext(d,th->mContext);
	XCloseDisplay(d);
	return NULL;
}
#endif

bool RenderThread::loadShaderPrograms()
{
	//Create render program
	assert(glCreateShader);
	GLuint f = glCreateShader(GL_FRAGMENT_SHADER);
	
	const char *fs = NULL;
	fs = dataFileRead("lightspark.frag");
	if(fs==NULL)
	{
		LOG(LOG_ERROR,_("Shader lightspark.frag not found"));
		throw RunTimeException("Fragment shader code not found");
	}
	assert(glShaderSource);
	glShaderSource(f, 1, &fs,NULL);
	free((void*)fs);

	bool ret=true;
	char str[1024];
	int a;
	assert(glCompileShader);
	glCompileShader(f);
	assert(glGetShaderInfoLog);
	glGetShaderInfoLog(f,1024,&a,str);
	LOG(LOG_NO_INFO,_("Fragment shader compilation ") << str);

	assert(glCreateProgram);
	gpu_program = glCreateProgram();
	assert(glAttachShader);
	glAttachShader(gpu_program,f);

	assert(glLinkProgram);
	glLinkProgram(gpu_program);
	assert(glGetProgramiv);
	glGetProgramiv(gpu_program,GL_LINK_STATUS,&a);
	if(a==GL_FALSE)
	{
		ret=false;
		return ret;
	}
	
	//Create the blitter shader
	GLuint v = glCreateShader(GL_VERTEX_SHADER);

	fs = dataFileRead("lightspark.vert");
	if(fs==NULL)
	{
		LOG(LOG_ERROR,_("Shader lightspark.vert not found"));
		throw RunTimeException("Vertex shader code not found");
	}
	glShaderSource(v, 1, &fs,NULL);
	free((void*)fs);

	glCompileShader(v);
	glGetShaderInfoLog(v,1024,&a,str);
	LOG(LOG_NO_INFO,_("Vertex shader compilation ") << str);

	blitter_program = glCreateProgram();
	glAttachShader(blitter_program,v);
	
	glLinkProgram(blitter_program);
	glGetProgramiv(blitter_program,GL_LINK_STATUS,&a);
	if(a==GL_FALSE)
	{
		ret=false;
		return ret;
	}

	assert(ret);
	return true;
}

float RenderThread::getIdAt(int x, int y)
{
	//TODO: use floating point textures
	uint32_t allocWidth=inputTex.getAllocWidth();
	return (interactive_buffer[y*allocWidth+x]&0xff)/255.0f;
}

void RenderThread::commonGLDeinit()
{
	//Fence any object that is still waiting for upload
	deque<ITextureUploadable*>::iterator it=uploadJobs.begin();
	for(;it!=uploadJobs.end();it++)
		(*it)->uploadFence();
	glBindFramebuffer(GL_FRAMEBUFFER,0);
	glDeleteFramebuffers(1,&rt->fboId);
	dataTex.shutdown();
	tempTex.shutdown();
	inputTex.shutdown();
	delete[] largeTextureBitmap;
	glDeleteTextures(1,&largeTextureId);
	glDeleteBuffers(2,pixelBuffers);
}

void RenderThread::commonGLInit(int width, int height)
{
	//Now we can initialize GLEW
	glewExperimental = GL_TRUE;
	GLenum err = glewInit();
	if (GLEW_OK != err)
	{
		LOG(LOG_ERROR,_("Cannot initialize GLEW: cause ") << glewGetErrorString(err));;
		::abort();
	}
	if(!GLEW_VERSION_2_0)
	{
		LOG(LOG_ERROR,_("Video card does not support OpenGL 2.0... Aborting"));
		::abort();
	}
	if(GLEW_ARB_texture_non_power_of_two)
		hasNPOTTextures=true;

	//Load shaders
	loadShaderPrograms();

	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_BLEND);

	glActiveTexture(GL_TEXTURE0);
	//Viewport setup and interactive_buffer allocation is left for GLResize	

	dataTex.init();

	tempTex.init(width, height, GL_NEAREST);

	inputTex.init(width, height, GL_NEAREST);

	//Set up the huge texture
	glGenTextures(1,&largeTextureId);
	assert(largeTextureId!=0);
	assert(glGetError()!=GL_INVALID_OPERATION);
	//If the previous call has not failed these should not fail (in specs, we trust)
	glBindTexture(GL_TEXTURE_2D,largeTextureId);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
	//Get the maximum allowed texture size, up to 1024
	int maxTexSize;
	glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTexSize);
	assert(maxTexSize>0);
	largeTextureSize=maxTexSize;
	//Allocate the texture
	while(glGetError()!=GL_NO_ERROR);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, largeTextureSize, largeTextureSize, 0, GL_BGRA, GL_UNSIGNED_BYTE, 0);
	if(glGetError())
	{
		LOG(LOG_ERROR,_("Can't allocate large texture... Aborting"));
		::abort();
	}
	//Let's allocate the bitmap for the texture blocks, minumum block size is 128
	uint32_t bitmapSize=(largeTextureSize/128)*(largeTextureSize/128)/8;
	largeTextureBitmap=new uint8_t[bitmapSize];
	memset(largeTextureBitmap,0,bitmapSize);

	//Create the PBOs
	glGenBuffers(2,pixelBuffers);

	//Set uniforms
	cleanGLErrors();
	glUseProgram(blitter_program);
	int texScale=glGetUniformLocation(blitter_program,"texScale");
	tempTex.setTexScale(texScale);
	cleanGLErrors();

	glUseProgram(gpu_program);
	cleanGLErrors();
	int tex=glGetUniformLocation(gpu_program,"g_tex1");
	glUniform1i(tex,0);
	fragmentTexScaleUniform=glGetUniformLocation(gpu_program,"texScale");
	glUniform2f(fragmentTexScaleUniform,1,1);
	cleanGLErrors();

	//Default to replace
	glTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE );
	// create a framebuffer object
	glGenFramebuffers(1, &fboId);
	glBindFramebuffer(GL_FRAMEBUFFER, fboId);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D, tempTex.getId(), 0);
	//Verify if we have more than an attachment available (1 is guaranteed)
	GLint numberOfAttachments=0;
	glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS, &numberOfAttachments);
	if(numberOfAttachments>=2)
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1,GL_TEXTURE_2D, inputTex.getId(), 0);
	else
	{
		LOG(LOG_ERROR,_("Non enough color attachments available, input disabled"));
		inputDisabled=true;
	}
	
	// check FBO status
	GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	if(status != GL_FRAMEBUFFER_COMPLETE)
	{
		LOG(LOG_ERROR,_("Incomplete FBO status ") << status << _("... Aborting"));
		while(err!=GL_NO_ERROR)
		{
			LOG(LOG_ERROR,_("GL errors during initialization: ") << err);
			err=glGetError();
		}
		::abort();
	}
}

void RenderThread::commonGLResize(int w, int h)
{
	//Get the size of the content
	RECT r=m_sys->getFrameSize();
	r.Xmax/=20;
	r.Ymax/=20;
	//Now compute the scalings and offsets
	switch(m_sys->scaleMode)
	{
		case SystemState::SHOW_ALL:
			//Compute both scaling
			scaleX=windowWidth;
			scaleX/=r.Xmax;
			scaleY=windowHeight;
			scaleY/=r.Ymax;
			//Enlarge with no distortion
			if(scaleX<scaleY)
			{
				//Uniform scaling for Y
				scaleY=scaleX;
				//Apply the offset
				offsetY=(windowHeight-r.Ymax*scaleY)/2;
				offsetX=0;
			}
			else
			{
				//Uniform scaling for X
				scaleX=scaleY;
				//Apply the offset
				offsetX=(windowWidth-r.Xmax*scaleX)/2;
				offsetY=0;
			}
			break;
		case SystemState::NO_BORDER:
			//Compute both scaling
			scaleX=windowWidth;
			scaleX/=r.Xmax;
			scaleY=windowHeight;
			scaleY/=r.Ymax;
			//Enlarge with no distortion
			if(scaleX>scaleY)
			{
				//Uniform scaling for Y
				scaleY=scaleX;
				//Apply the offset
				offsetY=(windowHeight-r.Ymax*scaleY)/2;
				offsetX=0;
			}
			else
			{
				//Uniform scaling for X
				scaleX=scaleY;
				//Apply the offset
				offsetX=(windowWidth-r.Xmax*scaleX)/2;
				offsetY=0;
			}
			break;
		case SystemState::EXACT_FIT:
			//Compute both scaling
			scaleX=windowWidth;
			scaleX/=r.Xmax;
			scaleY=windowHeight;
			scaleY/=r.Ymax;
			offsetX=0;
			offsetY=0;
			break;
		case SystemState::NO_SCALE:
			scaleX=1;
			scaleY=1;
			offsetX=0;
			offsetY=0;
			break;
	}
	glViewport(0,0,windowWidth,windowHeight);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0,windowWidth,0,windowHeight,-100,0);
	//scaleY is negated to adapt the flash and gl coordinates system
	//An additional translation is added for the same reason
	glTranslatef(offsetX,offsetY+windowHeight,0);
	glScalef(scaleX,-scaleY,1);

	glMatrixMode(GL_MODELVIEW);

	tempTex.resize(windowWidth, windowHeight);

	inputTex.resize(windowWidth, windowHeight);
	//Rellocate buffer for texture readback
	delete[] interactive_buffer;
	interactive_buffer=new uint32_t[inputTex.getAllocWidth()*inputTex.getAllocHeight()];
}

void RenderThread::requestResize(uint32_t w, uint32_t h)
{
	newWidth=w;
	newHeight=h;
	resizeNeeded=true;
	sem_post(&event);
}

void RenderThread::resizePixelBuffers(uint32_t w, uint32_t h)
{
	//Add enough room to realign to 16
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pixelBuffers[0]);
	glBufferData(GL_PIXEL_UNPACK_BUFFER, w*h*4+16, 0, GL_STREAM_DRAW);
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pixelBuffers[1]);
	glBufferData(GL_PIXEL_UNPACK_BUFFER, w*h*4+16, 0, GL_STREAM_DRAW);
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
	pixelBufferWidth=w;
	pixelBufferHeight=h;
}

void RenderThread::coreRendering(FTFont& font, bool testMode)
{
	//Now draw the input layer
	if(!inputDisabled)
	{
		glBindFramebuffer(GL_FRAMEBUFFER, fboId);
		glDrawBuffer(GL_COLOR_ATTACHMENT1);
		glClearColor(0,0,0,0);
		glClear(GL_COLOR_BUFFER_BIT);
	
		materialOverride=true;
		m_sys->inputRender();
		materialOverride=false;
	}
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glDrawBuffer(GL_BACK);
	//Clear the back buffer
	RGB bg=sys->getBackground();
	glClearColor(bg.Red/255.0F,bg.Green/255.0F,bg.Blue/255.0F,1);
	glClear(GL_COLOR_BUFFER_BIT);
	glLoadIdentity();

	if(m_sys->showInteractiveMap)
	{
		inputTex.bind();
		inputTex.setTexScale(fragmentTexScaleUniform);
		glColor4f(0,0,1,0);

		glBegin(GL_QUADS);
			glTexCoord2f(0,1);
			glVertex2i(0,0);
			glTexCoord2f(1,1);
			glVertex2i(windowWidth,0);
			glTexCoord2f(1,0);
			glVertex2i(windowWidth,windowHeight);
			glTexCoord2f(0,0);
			glVertex2i(0,windowHeight);
		glEnd();
	}
	else
		m_sys->Render();

	if(testMode && m_sys->showDebug)
	{
		glLoadIdentity();
		glScalef(1.0f/scaleX,-1.0f/scaleY,1);
		glTranslatef(-offsetX,(offsetY+windowHeight)*(-1.0f),0);
		glUseProgram(0);
		glDisable(GL_BLEND);
		glDisable(GL_TEXTURE_2D);
		if(selectedDebug)
			selectedDebug->debugRender(&font, true);
		else
			m_sys->debugRender(&font, true);
		glEnable(GL_TEXTURE_2D);
		glEnable(GL_BLEND);
		glUseProgram(gpu_program);
	}

	if(m_sys->showProfilingData)
	{
		glLoadIdentity();
		glScalef(1.0f/scaleX,-1.0f/scaleY,1);
		glTranslatef(-offsetX,(offsetY+windowHeight)*(-1.0f),0);
		glUseProgram(0);
		glDisable(GL_TEXTURE_2D);
		glColor3f(0,0,0);
		char frameBuf[20];
		snprintf(frameBuf,20,"Frame %u",m_sys->state.FP);
		font.Render(frameBuf,-1,FTPoint(0,0));

		//Draw bars
		glColor4f(0.7,0.7,0.7,0.7);
		glBegin(GL_LINES);
		for(int i=1;i<10;i++)
		{
			glVertex2i(0,(i*windowHeight/10));
			glVertex2i(windowWidth,(i*windowHeight/10));
		}
		glEnd();
		
		list<ThreadProfile>::iterator it=m_sys->profilingData.begin();
		for(;it!=m_sys->profilingData.end();it++)
			it->plot(1000000/m_sys->getFrameRate(),&font);
		glEnable(GL_TEXTURE_2D);
		glUseProgram(gpu_program);
	}
}

void* RenderThread::sdl_worker(RenderThread* th)
{
	sys=th->m_sys;
	rt=th;
	SemaphoreLighter lighter(th->initialized);
	RECT size=sys->getFrameSize();
	int initialWidth=size.Xmax/20;
	int initialHeight=size.Ymax/20;
	th->windowWidth=initialWidth;
	th->windowHeight=initialHeight;
	SDL_GL_SetAttribute( SDL_GL_RED_SIZE, 8 );
	SDL_GL_SetAttribute( SDL_GL_GREEN_SIZE, 8 );
	SDL_GL_SetAttribute( SDL_GL_BLUE_SIZE, 8 );
	SDL_GL_SetAttribute( SDL_GL_SWAP_CONTROL, 0 );
	SDL_GL_SetAttribute( SDL_GL_ACCELERATED_VISUAL, 1); 
	SDL_GL_SetAttribute( SDL_GL_DOUBLEBUFFER, 1 );

	SDL_SetVideoMode(th->windowWidth, th->windowHeight, 24, SDL_OPENGL|SDL_RESIZABLE);
	th->commonGLInit(th->windowWidth, th->windowHeight);
	th->commonGLResize(th->windowWidth, th->windowHeight);
	lighter.light();

	ThreadProfile* profile=sys->allocateProfiler(RGB(200,0,0));
	profile->setTag("Render");
	FTTextureFont font(th->fontPath.c_str());
	if(font.Error())
		throw RunTimeException("Unable to load font");
	
	font.FaceSize(12);
	try
	{
		//Texturing must be enabled otherwise no tex coord will be sent to the shader
		glEnable(GL_TEXTURE_2D);
		Chronometer chronometer;
		while(1)
		{
			sem_wait(&th->event);
			if(th->m_sys->isShuttingDown() || th->terminated)
				break;
			chronometer.checkpoint();

			if(th->resizeNeeded)
			{
				if(th->windowWidth!=th->newWidth ||
					th->windowHeight!=th->newHeight)
				{
					th->windowWidth=th->newWidth;
					th->windowHeight=th->newHeight;
					SDL_SetVideoMode(th->windowWidth, th->windowHeight, 24, SDL_OPENGL|SDL_RESIZABLE);
				}
				th->newWidth=0;
				th->newHeight=0;
				th->resizeNeeded=false;
				th->commonGLResize(th->windowWidth, th->windowHeight);
				continue;
			}

			if(th->inputNeeded)
			{
				th->inputTex.bind();
				glGetTexImage(GL_TEXTURE_2D,0,GL_BGRA,GL_UNSIGNED_BYTE,th->interactive_buffer);
				th->inputNeeded=false;
				sem_post(&th->inputDone);
			}

			if(th->prevUploadJob)
			{
				ITextureUploadable* u=th->prevUploadJob;
				glBindBuffer(GL_PIXEL_UNPACK_BUFFER, th->pixelBuffers[th->currentPixelBuffer]);
				//Copy content of the pbo to the texture, currentPixelBufferOffset is the offset in the pbo
				uint32_t w,h;
				u->sizeNeeded(w,h);
				th->loadChunkBGRA(u->getTexture(), w, h, (uint8_t*)th->currentPixelBufferOffset);
				glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
				u->uploadFence();
				th->prevUploadJob=NULL;
			}

			if(th->uploadNeeded)
			{
				ITextureUploadable* u=th->getUploadJob();
				if(u)
				{
					uint32_t w,h;
					u->sizeNeeded(w,h);
					if(w>th->pixelBufferWidth || h>th->pixelBufferHeight)
						th->resizePixelBuffers(w,h);
					//Increment and wrap current buffer index
					unsigned int nextBuffer = (th->currentPixelBuffer + 1)%2;

					glBindBuffer(GL_PIXEL_UNPACK_BUFFER, th->pixelBuffers[nextBuffer]);
					uint8_t* buf=(uint8_t*)glMapBuffer(GL_PIXEL_UNPACK_BUFFER,GL_WRITE_ONLY);
					uint8_t* alignedBuf=(uint8_t*)(uintptr_t((buf+15))&(~0xfL));

					u->upload(alignedBuf, w, h);

					glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
					glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

					th->currentPixelBufferOffset=alignedBuf-buf;
					th->currentPixelBuffer=nextBuffer;
					
					th->prevUploadJob=u;
					continue;
				}
				else if(!th->renderNeeded)
					continue;
			}

			assert(th->renderNeeded);

			SDL_PumpEvents();
			if(th->m_sys->isOnError())
			{
				glLoadIdentity();
				glScalef(1.0f/th->scaleX,-1.0f/th->scaleY,1);
				glTranslatef(-th->offsetX,(th->offsetY+th->windowHeight)*(-1.0f),0);
				glUseProgram(0);

				glBindFramebuffer(GL_FRAMEBUFFER, 0);
				glDrawBuffer(GL_BACK);

				glClearColor(0,0,0,1);
				glClear(GL_COLOR_BUFFER_BIT);
				glColor3f(0.8,0.8,0.8);
					    
				font.Render("We're sorry, Lightspark encountered a yet unsupported Flash file",
						-1,FTPoint(0,th->windowHeight/2));

				stringstream errorMsg;
				errorMsg << "SWF file: " << th->m_sys->getOrigin().getParsedURL();
				font.Render(errorMsg.str().c_str(),
						-1,FTPoint(0,th->windowHeight/2-20));
					    
				errorMsg.str("");
				errorMsg << "Cause: " << th->m_sys->errorCause;
				font.Render(errorMsg.str().c_str(),
						-1,FTPoint(0,th->windowHeight/2-40));

				font.Render("Press 'Q' to exit",-1,FTPoint(0,th->windowHeight/2-60));
				
				glFlush();
				SDL_GL_SwapBuffers( );
			}
			else
			{
				SDL_GL_SwapBuffers( );
				th->coreRendering(font, true);
				//Call glFlush to offload work on the GPU
				glFlush();
			}
			profile->accountTime(chronometer.checkpoint());
			th->renderNeeded=false;
		}
		glDisable(GL_TEXTURE_2D);
	}
	catch(LightsparkException& e)
	{
		LOG(LOG_ERROR,_("Exception in RenderThread ") << e.cause);
		sys->setError(e.cause);
	}
	th->commonGLDeinit();
	return NULL;
}

void RenderThread::addUploadJob(ITextureUploadable* u)
{
	Locker l(mutexUploadJobs);
	uploadJobs.push_back(u);
	uploadNeeded=true;
	sem_post(&event);
}

ITextureUploadable* RenderThread::getUploadJob()
{
	Locker l(mutexUploadJobs);
	if(uploadJobs.empty())
		return NULL;
	ITextureUploadable* ret=uploadJobs.front();
	uploadJobs.pop_front();
	return ret;
}

void RenderThread::draw()
{
	if(renderNeeded) //A rendering is already queued
		return;
	renderNeeded=true;
	sem_post(&event);
	time_d = compat_get_current_time_ms();
	uint64_t diff=time_d-time_s;
	if(diff>1000)
	{
		time_s=time_d;
		LOG(LOG_NO_INFO,_("FPS: ") << dec << frameCount);
		frameCount=0;
		secsCount++;
	}
	else
		frameCount++;
}

void RenderThread::tick()
{
	draw();
}

void RenderThread::releaseTexture(const TextureChunk& chunk)
{
	uint32_t blocksW=(chunk.width+127)/128;
	uint32_t blocksH=(chunk.height+127)/128;
	uint32_t numberOfBlocks=blocksW*blocksH;
	for(uint32_t i=0;i<numberOfBlocks;i++)
	{
		uint32_t bitOffset=chunk.chunks[i];
		assert(largeTextureBitmap[bitOffset/8]&(1<<(bitOffset%8)));
		largeTextureBitmap[bitOffset/8]^=(1<<(bitOffset%8));
	}
}

TextureChunk RenderThread::allocateTexture(uint32_t w, uint32_t h, bool compact)
{
	assert(w && h);
	Locker l(mutexLargeTexture);
	//Find the number of blocks needed for the given w and h
	uint32_t blocksW=(w+127)/128;
	uint32_t blocksH=(h+127)/128;
	TextureChunk ret(w, h);
	uint32_t blockPerSide=largeTextureSize/128;
	uint32_t bitmapSize=blockPerSide*blockPerSide;
	if(compact)
	{
		//Find a contiguos set of blocks
		uint32_t start;
		for(start=0;start<bitmapSize;start++)
		{
			bool badRect=false;
			for(uint32_t i=0;i<blocksH;i++)
			{
				for(uint32_t j=0;j<blocksW;j++)
				{
					uint32_t bitOffset=start+i*blockPerSide+j;
					if(largeTextureBitmap[bitOffset/8]&(1<<(bitOffset%8)))
					{
						badRect=true;
						break;
					}
				}
				if(badRect)
					break;
			}
			if(!badRect)
				break;
		}
		assert_and_throw(start<bitmapSize); //TODO: defragment and try again
		//Now set all those blocks are used
		for(uint32_t i=0;i<blocksH;i++)
		{
			for(uint32_t j=0;j<blocksW;j++)
			{
				uint32_t bitOffset=start+i*blockPerSide+j;
				largeTextureBitmap[bitOffset/8]|=1<<(bitOffset%8);
				ret.chunks[i*blocksW+j]=bitOffset;
			}
		}
	}
	else
	{
		//Allocate a sparse set of texture chunks
		uint32_t found=0;
		for(uint32_t i=0;i<bitmapSize;i++)
		{
			if((largeTextureBitmap[i/8]&(1<<(i%8)))==0)
			{
				largeTextureBitmap[i/8]|=1<<(i%8);
				ret.chunks[found]=i;
				found++;
				if(found==(blocksW*blocksH))
					break;
			}
		}
		assert(found==blocksW*blocksH);
		assert_and_throw(found==blocksW*blocksH);
	}
	return ret;
}

void RenderThread::renderTextured(const TextureChunk& chunk, uint32_t x, uint32_t y, uint32_t w, uint32_t h)
{
	glBindTexture(GL_TEXTURE_2D, largeTextureId);
	const uint32_t numberOfChunks=chunk.getNumberOfChunks();
	const uint32_t blocksPerSide=largeTextureSize/128;
	uint32_t startX, startY, endX, endY;
	assert(numberOfChunks==((chunk.width+127)/128)*((chunk.height+127)/128));
	
	uint32_t curChunk=0;
	for(uint32_t i=0;i<chunk.height;i+=128)
	{
		startY=h*i/chunk.height;
		endY=min(h*(i+128)/chunk.height,h);
		for(uint32_t j=0;j<chunk.width;j+=128)
		{
			startX=w*j/chunk.width;
			endX=min(w*(j+128)/chunk.width,w);
			const uint32_t curChunkId=chunk.chunks[curChunk];
			const uint32_t blockX=((curChunkId%blocksPerSide)*128);
			const uint32_t blockY=((curChunkId/blocksPerSide)*128);
			const uint32_t availX=min(int(chunk.width-j),128);
			const uint32_t availY=min(int(chunk.height-i),128);
			float startU=blockX;
			startU/=largeTextureSize;
			float startV=blockY;
			startV/=largeTextureSize;
			float endU=blockX+availX;
			endU/=largeTextureSize;
			float endV=blockY+availY;
			endV/=largeTextureSize;
			glBegin(GL_QUADS);
				glTexCoord2f(startU,startV);
				glVertex2i(x+startX,y+startY);

				glTexCoord2f(endU,startV);
				glVertex2i(x+endX,y+startY);

				glTexCoord2f(endU,endV);
				glVertex2i(x+endX,y+endY);

				glTexCoord2f(startU,endV);
				glVertex2i(x+startX,y+endY);
			glEnd();
			curChunk++;
		}
	}
}

void RenderThread::loadChunkBGRA(const TextureChunk& chunk, uint32_t w, uint32_t h, uint8_t* data)
{
	assert(w<=largeTextureSize && h<=largeTextureSize);
	glBindTexture(GL_TEXTURE_2D, largeTextureId);
	//TODO: Detect continuos
	assert(w==chunk.width);
	assert(h==chunk.height);
	const uint32_t numberOfChunks=chunk.getNumberOfChunks();
	const uint32_t blocksPerSide=largeTextureSize/128;
	const uint32_t blocksW=(w+127)/128;
	glPixelStorei(GL_UNPACK_ROW_LENGTH,w);
	for(uint32_t i=0;i<numberOfChunks;i++)
	{
		uint32_t curX=(i%blocksW)*128;
		uint32_t curY=(i/blocksW)*128;
		uint32_t sizeX=min(int(w-curX),128);
		uint32_t sizeY=min(int(h-curY),128);
		glPixelStorei(GL_UNPACK_SKIP_PIXELS,curX);
		glPixelStorei(GL_UNPACK_SKIP_ROWS,curY);
		const uint32_t blockX=((chunk.chunks[i]%blocksPerSide)*128);
		const uint32_t blockY=((chunk.chunks[i]/blocksPerSide)*128);
		while(glGetError()!=GL_NO_ERROR);
		glTexSubImage2D(GL_TEXTURE_2D, 0, blockX, blockY, sizeX, sizeY, GL_BGRA, GL_UNSIGNED_BYTE, data);
		assert(glGetError()!=GL_INVALID_OPERATION);
	}
	glPixelStorei(GL_UNPACK_SKIP_PIXELS,0);
	glPixelStorei(GL_UNPACK_SKIP_ROWS,0);
	glPixelStorei(GL_UNPACK_ROW_LENGTH,0);
}
