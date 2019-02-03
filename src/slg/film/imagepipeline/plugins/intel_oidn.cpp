#include "slg/film/imagepipeline/plugins/intel_oidn.h"

using namespace std;
using namespace luxrays;
using namespace slg;

ImagePipelinePlugin *IntelOIDN::Copy() const {
    return new IntelOIDN();
}

void IntelOIDN::Apply(Film &film, const u_int index) {
    Spectrum *pixels = (Spectrum *)film.channel_IMAGEPIPELINEs[index]->GetPixels();

    const size_t width = film.GetWidth();
    const size_t height = film.GetHeight();

    const u_int pixelCount = width * height;

    vector<float> color(3*pixelCount); // A float for each color channel
    vector<float> output(3*pixelCount); 

    for (u_int i = 0; i < pixelCount; i += 3) {
        color[i] = pixels[i].c[0];
        color[i + 1] = pixels[i].c[1];
        color[i + 2] = pixels[i].c[2];
	}

    oidn::DeviceRef device = oidn::newDevice();

    const char* errorMessage;
    if (device.getError(errorMessage) != oidn::Error::None)
      throw std::runtime_error(errorMessage);
    device.set("numThreads", -1); //Automatic for now
    device.commit();

    // TODO: HDR support
    oidn::FilterRef filter = device.newFilter("RT");
    filter.setImage("color", color.data(), oidn::Format::Float3, width, height);
    filter.setImage("output", output.data(), oidn::Format::Float3, width, height);
    filter.commit();

    filter.execute();

    for (u_int i = 0; i < pixelCount; i += 3) {
        pixels[i].c[0] = output[i];
        pixels[i].c[1] = output[i + 1];
        pixels[i].c[2] = output[i + 2];
	}


}