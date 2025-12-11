#include "Renderer.h"

Renderer::Renderer() : width(800),
height(800),
raeObj(nullptr),
renderScene(nullptr),
renderCam(nullptr),
FrameCount(1)
{
}
Renderer::~Renderer() {}

void Renderer::Init(Raether& rae, const Scene& scene, Camera& camera) {
	raeObj = &rae;
	renderCam = &camera;
	renderScene = &scene;

	SetBuffers();
}

void Renderer::SetBuffers() {
	width = renderCam->GetViewPortWidth();
	height = renderCam->GetViewPortHeight();

	/*
	ImageHorizontalIter.resize(width);

	for (uint32_t i = 0; i < width; i++) {
		ImageHorizontalIter[i] = i;
	}
	*/

	ImageVerticalIter.resize(height);
	
	for (uint32_t j = 0; j < height; j++) {
		ImageVerticalIter[j] = j;
	}

	ImageData.resize((uint64_t)(width * height));
	AccumImageData.resize((uint64_t)(width * height));
}

void Renderer::Render(const Scene& scene, Camera& camera) {

	if (FrameCount == 0) {
		/// Reset the Accumulate ImgaeBuffer
		memset(&AccumImageData[0], 0, width * height * sizeof(glm::dvec3));
	}

	else if (FrameCount < renderScene->GetSampleCount()) {

		#if MT == 1

		std::for_each(std::execution::par, ImageVerticalIter.begin(), ImageVerticalIter.end(),
			[this](uint32_t y) {
				for (uint32_t x = 0; x < width; x++) {

					/// Accumulate the color
					AccumImageData[x + y * width] += PerPixel(glm::dvec2(x, (height - 1) - y));

					glm::dvec3 accumColor = AccumImageData[(uint64_t)(x + y * width)];

					/// Adjust the brightness
					accumColor /= (double)FrameCount;

					accumColor = glm::clamp(accumColor, glm::dvec3(0.0), glm::dvec3(1.0));

					/// Store the color data
					ImageData[(uint64_t)(x + y * width)] = Utils::converttoRGBA(glm::dvec4(accumColor, 1.0));
				}
			});

		#else

		for (uint32_t y = 0; y < height; y++) {
			for (uint32_t x = 0; x < width; x++) {

				/// Accumulate the color
				AccumImageData[x + y * width] += PerPixel(glm::dvec2(x, (height - 1) - y));

				glm::dvec3 accumColor = AccumImageData[(uint64_t)(x + y * width)];

				/// Adjust the brightness
				accumColor /= (double)FrameCount;

				accumColor = glm::clamp(accumColor, glm::dvec3(0.0), glm::dvec3(1.0));

				/// Store the color data
				ImageData[(uint64_t)(x + y * width)] = Utils::converttoRGBA(glm::dvec4(accumColor, 1.0));
			}
		}

		#endif

		/// Draw the color using SDL
		raeObj->raeDrawImage(ImageData);
	}
	if (FrameCount <= renderScene->GetSampleCount()) {
		FrameCount++;
	}
}

glm::dvec3 Renderer::PerPixel(glm::dvec2 uv) {

	Ray ray;
	ray.Direction = renderCam->GetRayDirection()[(uint64_t)(uv.x + uv.y * renderCam->GetViewPortWidth())];
	ray.Time = renderCam->GetRayTime();

	if (renderCam->GetDefocusStrength() <= 0.0) {
		ray.Origin = renderCam->GetPosition();
	}
	else {
		glm::dvec3 focusPoint = renderCam->GetPosition() + ray.Direction * renderCam->GetFocusDistance();
		ray.Origin = renderCam->GetDefocusDiskSample();
		ray.Direction = glm::normalize(focusPoint - ray.Origin);
	}

	ray.Direction += Utils::RandomOffset(-JitterStrength, JitterStrength) * renderCam->GetCamFovFraction();

	Hitrec hitrecord;
	glm::dvec3 accumColor(0.0);        // Accumulated color
	glm::dvec3 attenuation(1.0);       // Current attenuation

	Interval rayhitdist = Interval(rayNearDist, rayFarDist);

	for (uint32_t bounces = 0; bounces < renderScene->GetSampleBounces(); bounces++) {

		if (renderScene->Hit(ray, rayhitdist, hitrecord)) {

			const std::shared_ptr<Material>& mat = hitrecord.MatId;
			glm::dvec3 scatterAttenuation;

			// Adding Emission Contribution
			glm::dvec3 Emission = mat->Emitted(hitrecord.U, hitrecord.V, hitrecord.HitPoint);
			accumColor += attenuation * Emission;

			// Adding Scatter Contribution
			if (mat->Scatter(ray, hitrecord, scatterAttenuation)) {
				attenuation *= scatterAttenuation;
			}
			else {
				break;
			}

		}
		else {
			accumColor += attenuation * Utils::Lerp(ray.Direction, renderScene->GetBackgroundColorNorth(), renderScene->GetBackgroundColorSouth());
			break;
		}
	}

	return accumColor;
}




























#if 0
//////////////////////////////////////////////////////////////////////////////////////////////////////
#define logtime std::chrono::steady_clock::now()
#define elapsed std::chrono::duration_cast<std::chrono::milliseconds>
#define clock std::chrono::steady_clock
// 
//Catch the start time
clock::time_point start = logtime;
// 
//Catch the end time
clock::time_point end = clock::now();
printf("Last Frame Renderer => %d\n", elapsed(end - start).count());
//////////////////////////////////////////////////////////////////////////////////////////////////////
#endif