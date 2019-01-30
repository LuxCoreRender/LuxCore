#ifndef _SLG_INTEL_OIDN_H
#define	_SLG_INTEL_OIDN_H

#include <boost/serialization/export.hpp>

#include <oidn/include/OpenImageDenoise/oidn.hpp>

#include "slg/film/film.h"
#include "slg/film/imagepipeline/imagepipeline.h"

namespace slg {

    class IntelOIDN : public ImagePipelinePlugin {
        public:

            virtual ImagePipelinePlugin *Copy() const;

            virtual void Apply(Film &film, const u_int index);
    };
}


BOOST_CLASS_VERSION(slg::IntelOIDN, 1)
BOOST_CLASS_EXPORT_KEY(slg::IntelOIDN)

#endif /* _SLG_INTEL_OIDN_H */