/*****************************************************************************/
/**
 * \file portable_archive_exception.hpp
 * \brief Provides error handling and constants.
 * \author christian.pfligersdorffer@gmx.at
 *
 * Portable archive exceptions derive from the boost archive exceptions
 * and add failure causes specific to the portable binary usecase.
 *
 * Additionally this header serves as common include for important
 * constants or typedefs.
 */
/****************************************************************************/

#pragma once

#include <boost/lexical_cast.hpp>
#include <boost/archive/basic_archive.hpp>
#include <boost/archive/archive_exception.hpp>

namespace eos {

	// this value is written to the top of the stream
	const signed char magic_byte = 'e' | 'o' | 's';

	// flag for fp serialization
	const unsigned no_infnan = 64;

	// integral type for the archive version
	#if BOOST_VERSION < 104400
		typedef boost::archive::version_type archive_version_type;
	#else
		typedef boost::archive::library_version_type archive_version_type;
	#endif

	// version of the linked boost archive library
	const archive_version_type archive_version(
	#if BOOST_VERSION < 103700
		boost::archive::ARCHIVE_VERSION()
	#else
		boost::archive::BOOST_ARCHIVE_VERSION()
	#endif
	);

	/**
	 * \brief Exception being thrown when serialization cannot proceed.
	 *
	 * There are several situations in which the portable archives may fail and
	 * hence throw an exception: 
	 * -# deserialization of an integer value that exceeds the range of the type 
	 * -# (de)serialization of inf/nan through an archive with no_infnan flag set
	 * -# deserialization of a denormalized value without the floating point type
	 *    supporting denormalized numbers
	 *
	 * Note that this exception will also be thrown if you mixed up your stream
	 * position and accidentially interpret some value for size data (in this case
	 * the reported size will be totally amiss most of the time).
	 */
	class portable_archive_exception : public boost::archive::archive_exception
	{
		std::string msg;

	public:
		//! type size is not large enough for deserialized number
		portable_archive_exception(signed char invalid_size) 
			: boost::archive::archive_exception(other_exception) 
			, msg("requested integer size exceeds type size: ")
		{
			msg += boost::lexical_cast<std::string, int>(invalid_size);
		}

		//! negative number in unsigned type
		portable_archive_exception()
			: boost::archive::archive_exception(other_exception)
			, msg("cannot read a negative number into an unsigned type")
		{
		}

		//! serialization of inf, nan and denormals
		template <typename T> 
		portable_archive_exception(const T& abnormal) 
			: boost::archive::archive_exception(other_exception) 
			, msg("serialization of illegal floating point value: ")
		{
			msg += boost::lexical_cast<std::string>(abnormal);
		}

		//! override the base class function with our message
		const char* what() const throw() { return msg.c_str(); }
		~portable_archive_exception() throw() {}
	};

} // namespace eos
