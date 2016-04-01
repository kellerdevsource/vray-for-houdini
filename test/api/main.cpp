#include <vraysdk.hpp>

#include <QtGui>


static VRay::VRayInit vrayInit(true);


static void onDumpMessage(VRay::VRayRenderer &/*renderer*/, const char *msg, int /*level*/, void* /*userData*/)
{
	QString message(msg);
	fprintf(stdout, "Message: %s\n",
			message.simplified().toAscii().constData());

	fflush(stdout);
}


static void onProgress(VRay::VRayRenderer &/*renderer*/, const char *msg, int elementNumber, int elementsCount, void* /*userData*/)
{
	const float percentage = 100.0 * elementNumber / elementsCount;

	QString message;
	message.sprintf("%s %.0f%%",
					msg, percentage);

	fprintf(stdout, "Progress: %s\n",
			message.simplified().toAscii().constData());

	fflush(stdout);
}


static void renderScene(const std::string &filepath)
{
	VRay::RendererOptions options;
	options.renderMode = VRay::RendererOptions::RENDER_MODE_PRODUCTION;
	options.keepRTRunning = true;
	options.imageWidth  = 640;
	options.imageHeight = 480;

	VRay::VRayRenderer *vray = new VRay::VRayRenderer(options);
	if (vray) {
		vray->setOnDumpMessage(onDumpMessage);
		vray->setOnProgress(onProgress);

		if (vray->load(filepath) == 0) {
			vray->start();
			vray->waitForImageReady();
		}
	}
}


int main(int argc, char *argv[])
{
	QApplication app(argc, argv);

	if (argc == 2) {
		const char *sceneFilePath = argv[1];

		renderScene(sceneFilePath);
	}

	return app.exec();
}
