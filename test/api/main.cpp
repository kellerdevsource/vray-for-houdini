#include <vraysdk.hpp>
#include <QtWidgets>


static VRay::VRayInit vrayInit(true);

#define MAGIC (('h'<<24)+('M'<<16)+('P'<<8)+('0'))






static void onDumpMessage(VRay::VRayRenderer &/*renderer*/, const char *msg, int /*level*/, void* /*userData*/)
{
	//QString message(msg);
	/*fprintf(stdout, "Message: %s\n",
			message.simplified().toAscii().constData());*/

	fflush(stdout);
}


static void onProgress(VRay::VRayRenderer &/*renderer*/, const char *msg, int elementNumber, int elementsCount, void* /*userData*/)
{
	const float percentage = 100.0 * elementNumber / elementsCount;

	QString message;
	message.sprintf("%s %.0f%%",
					msg, percentage);

	//fprintf(stdout, "Progress: %s\n",
			//message.simplified().toAscii().constData());

	fflush(stdout);
}


static void onImageReady(VRay::VRayRenderer &renderer, void* userData) {

	VRay::VRayImage *temp = renderer.getImage();
	
	int width, height;
	temp->getSize(width, height);
	const long size = width*height * sizeof(float) * 4;

	VRay::AColor* colourData = temp->getPixelData();
	//image obtained ^

	//currently the port used is: 53411, this is for the -s command
	//we should find a way to obtain the port number in stead of hard coding it in

	//open pipe v (start process with Qt)
	
	QObject *program;

	QString programPath = "C:/Program Files/Side Effects Software/Houdini 16.0.600/bin/imdisplay";
	QStringList arguments;

	arguments << "-k" << "-p" << "-n" << "\"TestWindow\"" << "-s" << "53411";

	QProcess *imDisplayProcess = new QProcess(program);
	imDisplayProcess->start(programPath, arguments);

	//sending data portion v

	int header[8] = { 0 };

	header[0] = MAGIC;
	header[1] = width;
	header[2] = height;

	//total number of channels
	//header[5] = 1;
	header[3] = 0;
	header[4] = 4;
	
	float *data = new float[4 * width*height];

	for (long i = 0; i < 4 * width*height; i+4) {
		data[i] = 0.763;
	}

	QByteArray byteArray(reinterpret_cast<const char*>(header), 32);
	
	imDisplayProcess->write(byteArray);

	QByteArray imageData(reinterpret_cast<const char*>(colourData));
	imDisplayProcess->write(imageData);




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
		vray->setOnImageReady(onImageReady);

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
