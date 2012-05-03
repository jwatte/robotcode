

#include <mrpt/slam/CObservation3DRangeScan.h>
//  work around a bug in Kinect headers
typedef mrpt::slam::CObservation3DRangeScan CObservation3DRangeScan;
#include <mrpt/hwdrivers/CCameraSensor.h>
#include <mrpt/utils/CConfigFile.h>
#include <mrpt/gui.h>
#include <mrpt/opengl.h>

#include <string>
#include <sstream>
#include <iostream>


int main(int argc, char const *argv[])
{
  mrpt::hwdrivers::CCameraSensor camRight;
  mrpt::hwdrivers::CCameraSensor camLeft;
  mrpt::utils::CConfigFile config("config.ini");
  camRight.loadConfig(config, "RIGHT");
  camLeft.loadConfig(config, "LEFT");
  camRight.initialize();
  camLeft.initialize();

  mrpt::gui::CDisplayWindow3D wnd("RIGHT", 1280, 720);
  mrpt::opengl::COpenGLViewportPtr viewport;
  {
    mrpt::opengl::COpenGLScenePtr scn(wnd.get3DSceneAndLock());
    scn->insert(mrpt::opengl::CGridPlaneXY::Create());
    viewport = scn->createViewport("capture");
    viewport->setViewportPosition(0, 0, 1280, 720);
    wnd.unlockAccess3DScene();
  }
  wnd.setPos(10, 10);

  while (wnd.isOpen())
  {
    mrpt::slam::CObservationPtr obsRight(camRight.getNextFrame());
    mrpt::slam::CObservationPtr obsLeft(camLeft.getNextFrame());
    mrpt::slam::CObservationImagePtr img(obsRight);
    wnd.get3DSceneAndLock();
    viewport->setImageView_fast(img->image);
    wnd.unlockAccess3DScene();
    wnd.repaint();
  }

  return 0;
}

