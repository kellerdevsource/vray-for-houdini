#include <vraysdk.hpp>

static void onProgress(VRay::VRayRenderer&, const char* msg, int elementNumber, int elementsCount, double instant, void*)
{
	printf("%s\n", msg);
}

static void onLogMessage(VRay::VRayRenderer&, const char* msg, VRay::MessageLevel level, double instant, void*)
{
	printf("%s\n", msg);
}

static void renderScene(const char *filepath)
{
	VRay::RendererOptions options;
	options.showFrameBuffer = false;

	VRay::VRayRenderer vray(options);
	vray.setRenderMode(VRay::VRayRenderer::RENDER_MODE_PRODUCTION);
	vray.setOnLogMessage(onLogMessage);
	vray.setOnProgress(onProgress);

	if (vray.load(filepath) == 0) {
		vray.start();
		vray.waitForRenderEnd();
	}
}

int main(int argc, char *argv[])
{
	VRay::VRayInit vrayInit(false);

	if (argc == 2) {
		const char *sceneFilePath = argv[1];
		renderScene(sceneFilePath);
	}

	return 0;
}
