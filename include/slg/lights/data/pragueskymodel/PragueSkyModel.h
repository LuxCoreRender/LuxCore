// Copyright 2022 Charles University
// SPDX-License-Identifier: Apache-2.0

#include <exception>
#include <string>
#include <vector>
#include <cmath>

double lerpPrague(const double from, const double to, const double factor);

/// Implementation of the physically-based sky model presented by Wilkie et al. [2021]
/// (https://cgg.mff.cuni.cz/publications/skymodel-2021/) and Vevoda et al. [2022]. Improves on previous work
/// especially in accuracy of sunset scenarios. Based on reconstruction of radiance from a small dataset
/// fitted to a large set of images obtained by brute force atmosphere simulation.
///
/// Provides evaluation of spectral sky radiance, sun radiance, transmittance and polarisation for observer at
/// a specific altitude above ground. The range of configurations depends on supplied dataset. The full
/// version models atmosphere of visibility (meteorological range) from 20 km to 131.8 km for sun elevations
/// from -4.2 degrees to 90 degrees, observer altitudes from 0 km to 15 km and ground albedo from 0 to 1, and
/// provides results for wavelengths from 280 nm to 2480 nm.
///
/// Usage:
/// 1. Create PragueSkyModel object and call its initialize method with a path to the dataset file.
/// 2. The model is parametrized by several values that are gathered in PragueSkyModel::Parameters structure.
/// You can either fill this structure manually (in that case see its description) or just call
/// computeParameters, which will compute it for you based on a few basic parameters.
/// 3. Use the Parameters structure when calling skyRadiance, sunRadiance, transmittance, or polarisation
/// methods to obtain the respective quantities.
///
/// Throws:
/// - DatasetNotFoundException: if the specified dataset file could not be found
/// - DatasetReadException: if an error occurred while reading the dataset file
/// - NoPolarisationException: if the polarisation method is called but the model does not contain
/// polarisation data
/// - NotInitializedException: if the model is used without calling the initialize method first
///
/// Note:
/// The entire model is written in a single class and does not depend on anything except of STL. It defines a
/// simple Vector3 class to simplify working with points and directions and expects using this class when
/// passing viewing point and direction to the computeParameters method.
class PragueSkyModel {


/////////////////////////////////////////////////////////////////////////////////////
// Public types
/////////////////////////////////////////////////////////////////////////////////////
public:
    /// Exception thrown by the initialize method if the passed dataset file could not be found.
    class DatasetNotFoundException : public std::exception {
    private:
        const std::string message;

    public:
        DatasetNotFoundException(const std::string& filename)
            : message(std::string("Dataset file ") + filename + std::string(" not found")) {}

        virtual const char* what() const throw() { return message.c_str(); }
    };

    /// Exception thrown by the initialize method if an error occurred while reading the passed dataset file.
    class DatasetReadException : public std::exception {
    private:
        const std::string message;

    public:
        DatasetReadException(const std::string& parameterName)
            : message(std::string("Dataset reading failed at ") + parameterName) {}

        virtual const char* what() const throw() { return message.c_str(); }
    };

    /// Exception thrown by the polarisation method if the dataset passed to the initialize method does not
    /// contain polarisation data.
    class NoPolarisationException : public std::exception {
    private:
        const std::string message;

    public:
        NoPolarisationException()
            : message(std::string("The supplied dataset does not contain polarisation data")) {}

        virtual const char* what() const throw() { return message.c_str(); }
    };

    /// Exception thrown when using the model without calling the initialize method first.
    class NotInitializedException : public std::exception {
    private:
        const std::string message;

    public:
        NotInitializedException()
            : message(std::string("The model is not initialized")) {}

        virtual const char* what() const throw() { return message.c_str(); }
    };

    /// A simple 3D vector implementation. Provides some basic operations.
    class Vector3 {
    public:
        double x, y, z;

        Vector3() {
            this->x = 0.0;
            this->y = 0.0;
            this->z = 0.0;
        }

        Vector3(double x, double y, double z) {
            this->x = x;
            this->y = y;
            this->z = z;
        }

        Vector3 operator+(const Vector3& other) const {
            return Vector3(x + other.x, y + other.y, z + other.z);
        }
        Vector3 operator-(const Vector3& other) const {
            return Vector3(x - other.x, y - other.y, z - other.z);
        }

        Vector3 operator*(const double factor) const { return Vector3(x * factor, y * factor, z * factor); }

        Vector3 operator/(const double factor) const { return Vector3(x / factor, y / factor, z / factor); }

        bool isZero() const { return x == 0.0 && y == 0.0 && z == 0.0; }
    };

    /// Structure holding all parameters necessary for querying the model.
    struct Parameters {
        /// Angle between view direction and direction to zenith in radians, supported values in range [0,
        /// PI].
        double theta;

        /// Angle between view direction and direction to sun in radians, supported values in range [0, PI].
        double gamma;

        /// Altitude-corrected angle between view direction and direction perpendicular to a shadow plane (=
        /// direction to sun rotated PI / 2 towards direction to zenith) in radians, used for negative solar
        /// elevations only, supported values in range [0, PI]
        double shadow;

        /// Altitude-corrected version of the theta angle in radians, supported values in range [0, PI].
        double zero;

        /// Sun elevation at view point in radians, supported values in range [-0.073, PI/2] (for full
        /// dataset). For view points above ground differs from the ground level sun elevation expected by the
        /// computeParameters method.
        double elevation;

        /// Altitude of view point in meters, supported values in range [0, 15000] (for full dataset).
        double altitude;

        /// Horizontal visibility (meteorological range) at ground level in kilometers, supported values in
        /// range [20, 131.8] (for full dataset).
        double visibility;

        /// Ground albedo, supported values in range [0, 1] (for full dataset).
        double albedo;
    };

    /// Structure with parameter ranges available in currently loaded dataset.
    struct AvailableData {
        double albedoMin;
        double albedoMax;
        double altitudeMin;
        double altitudeMax;
        double elevationMin;
        double elevationMax;
        double visibilityMin;
        double visibilityMax;
        bool   polarisation;
        int    channels;
        double channelStart;
        double channelWidth;
    };


/////////////////////////////////////////////////////////////////////////////////////
// Private types
/////////////////////////////////////////////////////////////////////////////////////
private:
    /// Structure holding index and factor for interpolating with respect to two neighboring values of an
    /// array.
    struct InterpolationParameter {
        double factor;
        int    index;
    };

    /// Angles converted into interpolation parameters.
    struct AngleParameters {
        InterpolationParameter gamma, alpha, zero;
    };

    /// Structure controlling interpolation with respect to visibility, albedo, altitude and elevation.
    struct ControlParameters {
        /// 16 sets of parameters that will be bi-linearly interpolated
        std::array<std::vector<float>::const_iterator, 16> coefficients;
        std::array<double, 4>                              interpolationFactor;
    };

    /// Structure used for storing radiance and polarisation metadata.
    struct Metadata {
        int rank;

        int                 sunOffset;
        int                 sunStride;
        std::vector<double> sunBreaks;

        int                 zenithOffset;
        int                 zenithStride;
        std::vector<double> zenithBreaks;

        int                 emphOffset; // not used for polarisation
        std::vector<double> emphBreaks; // not used for polarisation

        int totalCoefsSingleConfig;
        int totalCoefsAllConfigs;
	};

	/// Transmittance model internal parameters.
	struct TransmittanceParameters {
		InterpolationParameter altitude;
		InterpolationParameter distance;
	};


/////////////////////////////////////////////////////////////////////////////////////
// Private data
/////////////////////////////////////////////////////////////////////////////////////
    int    channels;
    double channelStart;
    double channelWidth;

    bool initialized;

	// Total number of configurations
	int totalConfigs;

    // Number of configurations skipped from the beginning of the radiance/polarisation coefficients part 
    // of the dataset file (if loading of just one visibility was requested)
    int skippedConfigsBegin;

    // Number of configurations skipped till the end of the radiance/polarisation coefficients part
    // of the dataset file (if loading of just one visibility was requested)
    int skippedConfigsEnd;

    // Metadata common for radiance and polarisation

    std::vector<double> visibilitiesRad;
    std::vector<double> albedosRad;
    std::vector<double> altitudesRad;
    std::vector<double> elevationsRad;

    // Radiance metadata

    Metadata metadataRad;

    // Radiance data
    // 
	// Structure:
	// [[[[[[ sunCoefsRad       (sunBreaksCountRad * float), 
	//        zenithCoefsRad (zenithBreaksCountRad * float) ] * rankRad, 
	//        emphCoefsRad     (emphBreaksCountRad * float) ]
	//  * channels ] * elevationCount ] * altitudeCount ] * albedoCount ] * visibilityCount
    
    std::vector<float> dataRad;

	// Polarisation metadata

    Metadata metadataPol;

	// Polarisation data
    // 
	// Struture:
	// [[[[[[ sunCoefsPol       (sunBreaksCountPol * float), 
	//        zenithCoefsPol (zenithBreaksCountPol * float) ] * rankPol] 
	// * channels ] * elevationCount ] * altitudeCount ] * albedoCount ] * visibilityCount

    std::vector<float> dataPol;

    // Transmittance metadata

    int                 aDim;
    int                 dDim;
    int                 rankTrans;
    std::vector<double> altitudesTrans;
    std::vector<double> visibilitiesTrans;

    // Transmittance data

    std::vector<float> dataTransU;
    std::vector<float> dataTransV;


/////////////////////////////////////////////////////////////////////////////////////
// Public methods
/////////////////////////////////////////////////////////////////////////////////////
public:
    PragueSkyModel() : initialized(false) {};

    /// Prepares the model and loads the given dataset file into memory.
    ///
    /// If a positive visibility value is passed, only a portion of the dataset needed for evaluation of that
    /// particular visibility is loaded (two nearest visibilities are loaded if the value is included in the
    /// dataset or one nearest if not). Otherwise, the entire dataset is loaded.
    ///
    /// Throws:
    /// - DatasetNotFoundException: if the specified dataset file could not be found
    /// - DatasetReadException: if an error occurred while reading the dataset file
    void initialize(const std::string& filename, const double singleVisibility = 0.0);

    bool isInitialized() const { return initialized; }

    /// Gets parameter ranges available in currently loaded dataset.
    ///
    /// Throws NotInitializedException if called without initializing the model first.
    AvailableData getAvailableData() const;

    /// Computes all the parameters in the Parameters structure necessary for querying the model.
    ///
    /// Expects view point and direction, sun elevation and azimuth at origin, ground level visibility and
    /// ground albedo. Assumes origin at [0,0,0] with Z axis pointing up. Thus view point [0, 0, 100] defines
    /// observer altitude 100 m. Range of available values depends on the used dataset. The full version
    /// supports altitude from [0, 15000] m, elevation from [-0.073, PI/2] rad, azimuth from [0, PI] rad, visibility
    /// from [20, 131.8] km, and albedo from [0, 1]. Values outside range of the used dataset are clamped to the
    /// nearest supported value.
    Parameters computeParameters(const Vector3& viewPoint,
                                 const Vector3& viewDirection,
                                 const double   groundLevelSolarElevationAtOrigin,
                                 const double   groundLevelSolarAzimuthAtOrigin,
                                 const double   visibility,
                                 const double   albedo) const;

    /// Computes sky radiance only (without direct sun contribution) for given parameters and wavelength (full
    /// dataset supports wavelengths from 280 nm to 2480 nm).
    ///
    /// Throws NotInitializedException if called without initializing the model first.
    double skyRadiance(const Parameters& params, const double wavelength) const;

    /// Computes sun radiance only (without radiance inscattered from the sky) for given parameters and
    /// wavelength (full dataset supports wavelengths from 280 nm to 2480 nm).
    ///
    /// Checks whether the parameters correspond to view direction hitting the sun and returns 0 if not.
    ///
    /// Throws NotInitializedException if called without initializing the model first.
    double sunRadiance(const Parameters& params, const double wavelength) const;

    /// Computes degree of polarisation for given parameters and wavelength (full
    /// dataset supports wavelengths from 280 nm to 2480 nm). Can be negative.
    ///
    /// Throws:
    /// - NoPolarisationException: if the polarisation method is called but the model does not contain
    /// polarisation data
    /// - NotInitializedException: if called without initializing the model first
    double polarisation(const Parameters& params, const double wavelength) const;

    /// Computes transmittance between view point and a point certain distance away from it along view
    /// direction.
    ///
    /// Expects the Parameters structure, wavelength (full dataset supports wavelengths from 280 nm
    /// to 2480 nm) and the distance (any positive number, use std::numeric_limits<double>::max() for
    /// infinity).
    ///
    /// Throws NotInitializedException if called without initializing the model first.
    double transmittance(const Parameters& params, const double wavelength, const double distance) const;


/////////////////////////////////////////////////////////////////////////////////////
// Private methods
/////////////////////////////////////////////////////////////////////////////////////
private:
    /// Reads radiance part of the dataset file into memory.
    ///
    /// If a positive visibility value is passed, only a portion of the dataset needed for evaluation of that
    /// particular visibility is loaded (two nearest visibilities are loaded if the value is included in the
    /// dataset or one nearest if not). Otherwise, the entire dataset is loaded.
    ///
    /// Throws DatasetReadException if an error occurred while reading the dataset file.
    void readRadiance(FILE* handle, const double singleVisibility);

    /// Reads transmittance part of the dataset file into memory.
    /// Throws DatasetReadException if an error occurred while reading the dataset file.
    void readTransmittance(FILE* handle);

    /// Reads polarisation part of the dataset file into memory.
    /// Throws DatasetReadException if an error occurred while reading the dataset file.
    void readPolarisation(FILE* handle);

    // Sky radiance and polarisation

    /// Gets iterator to coefficients in the dataset array corresponding to the given configuration. Used for
    /// sky radiance and polarisation.
    std::vector<float>::const_iterator getCoefficients(const std::vector<float>& dataset,
                                                       const int                 totalCoefsSingleConfig,
                                                       const int                 elevation,
                                                       const int                 altitude,
                                                       const int                 visibility,
                                                       const int                 albedo,
                                                       const int                 wavelength) const;  

    /// Recursive function controlling interpolation of reconstructed radiance between two neighboring visibility,
    /// albedo, altitude and elevation values.
    template <int TOffset, int TLevel>
    double interpolate(const AngleParameters&   angleParameters,
                       const ControlParameters& controlParameters,
                       const Metadata&          metadata) const {
        /// Starts at level 0 and recursively goes down to level 4 while computing offset to the control
        /// parameters array. There it reconstructs radiance. When returning from recursion interpolates
        /// according to elevation, altitude, albedo and visibility at level 3, 2, 1 and 0, respectively.
        if constexpr (TLevel == 4) {
            return reconstruct(angleParameters, controlParameters.coefficients[TOffset], metadata);
        } else {
            // Compute the first value
            const double resultLow =
                interpolate<TOffset, TLevel + 1>(angleParameters, controlParameters, metadata);

            // Skip the second value if not useful or not available.
            if (controlParameters.interpolationFactor[TLevel] < 1e-6) {
                return resultLow;
            }

            // Compute the second value
            const double resultHigh =
                interpolate<TOffset + (1 << (3 - TLevel)), TLevel + 1>(angleParameters,
                                                                       controlParameters,
                                                                       metadata);

            /// Interpolate between the two
            return lerpPrague(resultLow, resultHigh, controlParameters.interpolationFactor[TLevel]);
        }
    }

    /// Reconstructs sky radiance or polarisation from the given control parameters by inverse tensor
    /// decomposition.
    double reconstruct(const AngleParameters&                   angleParameters,
                       const std::vector<float>::const_iterator controlParameters,
                       const Metadata&                          metadata) const;

    /// Get interpolation parameter for the given query value, i.e. finds position of the query value between
    /// a pair of break values.
    ///
    /// Used for albedo, elevation, altitude, visibility and angles (theta, alpha, or gamma).
    InterpolationParameter getInterpolationParameter(const double               queryVal,
                                                     const std::vector<double>& breaks) const;
    
    /// Evaluates the model. Used for computing sky radiance and polarisation.
    double evaluateModel(const Parameters&         params,
                         const double              wavelength,
                         const std::vector<float>& data,
                         const Metadata&           metadata) const;

    // Transmittance

    /// Gets iterator to base transmittance coefficients in the dataset array corresponding to the given
    /// configuration.
    std::vector<float>::const_iterator getCoefficientsTransBase(const int altitude,
                                                                const int a,
                                                                const int d) const;

    /// Gets iterator to transmittance coefficients in the dataset array corresponding to the given
    /// configuration.
    std::vector<float>::const_iterator getCoefficientsTrans(const int visibility,
                                                            const int altitude,
                                                            const int wavelength) const;

    /// Interpolates transmittances computed for two nearest altitudes.
    double interpolateTrans(const int                     visibilityIndex,
                            const InterpolationParameter  altitudeParam,
                            const TransmittanceParameters transParams,
                            const int                     channelIndex) const;

    /// For each channel reconstructs transmittances for the four nearest transmittance parameters and
    /// interpolates them.
    double reconstructTrans(const int                     visibilityIndex,
                            const int                     altitudeIndex,
                            const TransmittanceParameters transParams,
                            const int                     channelIndex) const;

    /// Converts altitude or distance value used in the transmittance model into interpolation parameter.
    InterpolationParameter getInterpolationParameterTrans(const double value,
                                                          const int    paramCount,
                                                          const int    power) const;

    /// Transforms the given theta angle, observer altitude and distance along ray into transmittance model
    /// internal [altitude, distance] parametrization.
    TransmittanceParameters toTransmittanceParams(const double theta,
                                                  const double distance,
                                                  const double altitude) const;
};