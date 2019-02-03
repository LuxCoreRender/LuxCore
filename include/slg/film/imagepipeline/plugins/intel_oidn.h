#ifndef _SLG_INTEL_OIDN_H
#define	_SLG_INTEL_OIDN_H

#include <vector>

#include <boost/serialization/export.hpp>

#include <OpenImageDenoise/oidn.hpp>

#include "luxrays/luxrays.h"
#include "luxrays/core/color/color.h"
#include "slg/film/film.h"
#include "slg/film/imagepipeline/imagepipeline.h"

namespace slg {

    class IntelOIDN : public ImagePipelinePlugin {
        public:

            virtual ImagePipelinePlugin *Copy() const;

            virtual void Apply(Film &film, const u_int index);

            friend class boost::serialization::access;
        
        private:

            template<class Archive> void serialize(Archive &ar, const u_int version) {
                ar & BOOST_SERIALIZATION_BASE_OBJECT_NVP(ImagePipelinePlugin);
                // TODO: implement serialization for ImageMap
                throw std::runtime_error("IntelOIDN serialization not yet supported");
            }
    };
}


BOOST_CLASS_VERSION(slg::IntelOIDN, 1)
BOOST_CLASS_EXPORT_KEY(slg::IntelOIDN)

#endif /* _SLG_INTEL_OIDN_H */