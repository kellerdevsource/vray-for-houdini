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

static void writeTile(const int &x0, 
	const int &x1, 
	const int &y0, 
	const int &y1, 
	const int *colours,
	const int &cpos,
	QProcess *process) {

	int tileHeader[4] = { 0 };
	tileHeader[0] = x0;
	tileHeader[1] = x1 - 1;
	tileHeader[2] = y0;
	tileHeader[3] = y1 - 1;

	const int tileHeaderSize = _countof(tileHeader) * sizeof(int);
	QByteArray tileHeaderByteArray(reinterpret_cast<const char*>(tileHeader), tileHeaderSize);
	if (process->write(reinterpret_cast<const char*>(tileHeader), tileHeaderSize) != tileHeaderSize) {
		Q_ASSERT(false);
	}

	

	const int size = (x1 - x0) + (y1 - y0);

	int colourValue[4] = { colours[cpos], 
		colours[cpos + 1], 
		colours[cpos + 2], 
		colours[cpos + 3] 
	};

	const int colourValueSize = _countof(colourValue) * sizeof(int);
	QByteArray colourArray(reinterpret_cast<const char*>(colourValue), colourValueSize);
	for (int i = 0; i < size; i++) {
		if (process->write(reinterpret_cast<const char*>(colourValue), colourValueSize) != colourValueSize) {
			Q_ASSERT(false);
		}

	}
}

static void onImageReady(VRay::VRayRenderer &renderer, void* userData) {

	VRay::VRayImage *temp = renderer.getImage();

	int width, height;
	temp->getSize(width, height);
	const long size = width*height * sizeof(float) * 4;

	VRay::AColor* colourData = temp->getPixelData();
	//image obtained ^
	//open pipe v (start process with Qt)

	QObject *program = nullptr;

	QString programPath = "C:/Program Files/Side Effects Software/Houdini 16.0.600/bin/imdisplay.exe";
	QStringList arguments;

	arguments << "-p" << "-k" << "-s" << "58121";// << "-n" << "\"TestWindow\"";

	QProcess *imDisplayProcess = new QProcess(program);
	imDisplayProcess->start(programPath, arguments);


	QProcess::ProcessError errorCode = imDisplayProcess->error();
	//sending data portion v

	int header[8] = { 0 };

	header[0] = MAGIC;
	header[1] = width;
	header[2] = height;

	//total number of channels
	//header[5]

	//format
	//0-floating point data
	//1-unsigned char data
	//2-unsigned short data
	//4-unsigned int data
	header[3] = 4;
	//size of array
	//4-RGBA
	//3-RGB
	//1-single channel image
	header[4] = 4;

	int colours[32] = { 0, 0, 0, 255,
					255, 0, 0, 255,
					0, 255, 0, 255,
					0, 0, 255, 255,
					255, 255, 0, 255,
					0, 255, 255, 255,
					255, 0, 255, 255,
					255, 255, 255, 255 };

	const int headerSize = _countof(header) * sizeof(int);
	QByteArray byteArray(reinterpret_cast<const char*>(header), headerSize);

	if (imDisplayProcess->write(reinterpret_cast<const char*>(header), headerSize) != headerSize) {
		Q_ASSERT(false);
	}
	//write the Tile header (treating entire image as one tile)
	errorCode = imDisplayProcess->error();
	

	while (true) {
		writeTile(0, width, 0, height, colours, 24, imDisplayProcess);
		Sleep(100);
		printf("wrote once");
	}
	errorCode = imDisplayProcess->error();
	/*QByteArray imageData(reinterpret_cast<const char*>(colours), 36*sizeof(int));
	if (imDisplayProcess->write(imageData) != 36 * sizeof(int)) {
		Q_ASSERT(false);
	}*/
	//imDisplayProcess->close();

	delete temp;
}

static void renderScene(const std::string &filepath)
{
	VRay::RendererOptions options;
	options.renderMode = VRay::RendererOptions::RENDER_MODE_PRODUCTION;
	options.keepRTRunning = true;
	options.imageWidth  = 640;
	options.imageHeight = 480;
	options.rtNoiseThreshold = 0.5f;

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
