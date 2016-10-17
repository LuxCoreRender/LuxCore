/*****************************************************************************/
/**
 * \file portable_iarchive.hpp
 * \brief Provides an archive to read from portable binary files.
 * \author christian.pfligersdorffer@gmx.at
 * \version 5.1
 *
 * This pair of archives brings the advantages of binary streams to the cross
 * platform boost::serialization user. While being almost as fast as the native
 * binary archive it allows its files to be exchanged between cpu architectures
 * using different byte order (endianness). Speaking of speed: in serializing
 * numbers the (portable) binary approach is approximately ten times faster than
 * the ascii implementation (that is inherently portable)!
 *
 * Based on the portable archive example by Robert Ramey this implementation
 * uses Beman Dawes endian library and fp_utilities from Johan Rade, both being
 * in boost since 1.36. Prior to that you need to add them both (header only)
 * to your boost directory before you're able to use the archives provided. 
 * Our archives have been tested successfully for boost versions 1.33 to 1.49!
 *
 * \note Correct behaviour has so far been confirmed using PowerPC-32, x86-32
 *       and x86-64 platforms featuring different byte order. So there is a good
 *       chance it will instantly work for your specific setup. If you encounter
 *       problems or have suggestions please contact the author.
 *
 * \note Version 5.1 is now compatible with boost up to version 1.59. Thanks to
 *       ecotax for pointing to the issue with shared_ptr_helper.
 *
 * \note Version 5.0 is now compatible with boost up to version 1.49 and enables
 *       serialization of std::wstring by converting it to/from utf8 (thanks to
 *       Arash Abghari for this suggestion). With that all unit tests from the
 *       serialization library pass again with the notable exception of user
 *       defined primitive types. Those are not supported and as a result any
 *       user defined type to be used with the portable archives are required 
 *       to be at least object_serializable.
 *
 * \note Version 4.2 maintains compatibility with the latest boost 1.45 and adds
 *       serialization of special floating point values inf and NaN as proposed
 *       by Francois Mauger.
 *
 * \note Version 4.1 makes the archives work together with boost 1.40 and 1.41.
 *       Thanks to Francois Mauger for his suggestions.
 *
 * \note Version 4 removes one level of the inheritance hierarchy and directly
 *       builds upon binary primitive and basic binary archive, thereby fixing
 *       the last open issue regarding array serialization. Thanks to Robert
 *       Ramey for the hint.
 *
 * \note A few fixes introduced in version 3.1 let the archives pass all of the
 *       serialization tests. Thanks to Sergey Morozov for running the tests.
 *       Wouter Bijlsma pointed out where to find the fp_utilities and endian
 *       libraries headers inside the boost distribution. I would never have
 *       found them so thank him it works out of the box since boost 1.36.
 *
 * \note With Version 3.0 the archives have been made portable across different
 *       boost versions. For that purpose a header is added to the data that
 *       supplies the underlying serialization library version. Backwards
 *       compatibility is maintained by assuming library version boost 1.33 if
 *       the iarchive is created using the no_header flag. Whether a header is
 *       present or not can be guessed by peeking into the stream: the header's
 *       first byte is the magic number 127 coinciding with 'e'|'o'|'s' :-)
 *
 * \note Version 2.1 removes several compiler warnings and enhances floating
 *       point diagnostics to inform the user if some preconditions are violated
 *		 on his platform. We do not strive for the universally portable solution
 *       in binary floating point serialization as desired by some boost users.
 *       Instead we support only the most widely used IEEE 754 format and try to
 *       detect when requirements are not met and hence our approach must fail.
 *       Contributions we made by Johan Rade and Ákos Maróy.
 *
 * \note Version 2.0 fixes a serious bug that effectively transformed most
 *       of negative integral values into positive values! For example the two
 *       numbers -12 and 234 were stored in the same 8-bit pattern and later
 *       always restored to 234. This was fixed in this version in a way that
 *       does not change the interpretation of existing archives that did work
 *       because there were no negative numbers. The other way round archives
 *       created by version 2.0 and containing negative numbers will raise an
 *       integer type size exception when reading it with version 1.0. Thanks
 *       to Markus Frohnmaier for testing the archives and finding the bug.
 *
 * \copyright The boost software license applies.
 */
/*****************************************************************************/

#pragma once

#include <istream>

// basic headers
#include <boost/version.hpp>
#include <boost/utility/enable_if.hpp>
#include <boost/archive/basic_binary_iprimitive.hpp>
#include <boost/archive/basic_binary_iarchive.hpp>

#if BOOST_VERSION >= 103500 && BOOST_VERSION < 105600
#include <boost/archive/shared_ptr_helper.hpp>
#endif

// funny polymorphics
#if BOOST_VERSION < 103500
#include <boost/archive/detail/polymorphic_iarchive_impl.hpp>
#define POLYMORPHIC(T) boost::archive::detail::polymorphic_iarchive_impl<T>

#elif BOOST_VERSION < 103600
#include <boost/archive/detail/polymorphic_iarchive_dispatch.hpp>
#define POLYMORPHIC(T) boost::archive::detail::polymorphic_iarchive_dispatch<T>

#else
#include <boost/archive/detail/polymorphic_iarchive_route.hpp>
#define POLYMORPHIC(T) boost::archive::detail::polymorphic_iarchive_route<T>
#endif

// endian and fpclassify
#if BOOST_VERSION < 103600
#include <boost/integer/endian.hpp>
#include <boost/math/fpclassify.hpp>
#elif BOOST_VERSION < 104800
#include <boost/spirit/home/support/detail/integer/endian.hpp>
#include <boost/spirit/home/support/detail/math/fpclassify.hpp>
#else
#include <boost/spirit/home/support/detail/endian/endian.hpp>
#include <boost/spirit/home/support/detail/math/fpclassify.hpp>
#endif

// namespace alias
#if BOOST_VERSION < 103800
namespace fp = boost::math;
#else
namespace fp = boost::spirit::math;
#endif

// namespace alias endian
#if BOOST_VERSION < 104800
namespace endian = boost::detail;
#else
namespace endian = boost::spirit::detail;
#endif

#if BOOST_VERSION >= 104500 && !defined BOOST_NO_STD_WSTRING
// used for wstring to utf8 conversion
#include <boost/program_options/config.hpp>
#include <boost/program_options/detail/convert.hpp>
#endif

// generic type traits for numeric types
#include <boost/type_traits/is_integral.hpp>
#include <boost/type_traits/is_unsigned.hpp>
#include <boost/type_traits/is_arithmetic.hpp>
#include <boost/type_traits/is_floating_point.hpp>

#include "portable_archive_exception.hpp"

// hint from Johan Rade: on VMS there is still support for
// the VAX floating point format and this macro detects it
#if defined(__vms) && defined(__DECCXX) && !__IEEE_FLOAT
#error "VAX floating point format is not supported!"
#endif

namespace eos {

	// forward declaration
	class portable_iarchive;

	typedef boost::archive::basic_binary_iprimitive<
		portable_iarchive
	#if BOOST_VERSION < 103400
		, std::istream
	#else
		, std::istream::char_type 
		, std::istream::traits_type
	#endif
	> portable_iprimitive;

	/**
	 * \brief Portable binary input archive using little endian format.
	 *
	 * This archive addresses integer size, endianness and floating point types so
	 * that data can be transferred across different systems. There may still be
	 * constraints as to what systems are compatible and the user will have to take
	 * care that e.g. a very large int being saved on a 64 bit machine will result
	 * in a portable_archive_exception if loaded into an int on a 32 bit system.
	 * A possible workaround to this would be to use fixed types like
	 * boost::uint64_t in your serialization structures.
	 *
	 * \note The class is based on the portable binary example by Robert Ramey and
	 *	     uses Beman Dawes endian library plus fp_utilities by Johan Rade.
	 */
	class portable_iarchive : public portable_iprimitive

		// the example derives from common_oarchive but that lacks the
		// load_override functions so we chose to stay one level higher
		, public boost::archive::basic_binary_iarchive<portable_iarchive>

	#if BOOST_VERSION >= 103500 && BOOST_VERSION < 105600
		// mix-in helper class for serializing shared_ptr
		, public boost::archive::detail::shared_ptr_helper
	#endif
	{
		// only needed for Robert's hack in basic_binary_iarchive::init
		friend class boost::archive::basic_binary_iarchive<portable_iarchive>;

		// workaround for gcc: use a dummy struct
		// as additional argument type for overloading
		template <int> struct dummy { dummy(int) {}};

		// loads directly from stream
		inline signed char load_signed_char()
		{ 
			signed char c; 
			portable_iprimitive::load(c); 
			return c; 
		}

		// archive initialization
		void init(unsigned flags)
		{
			using namespace boost::archive;
			archive_version_type input_library_version(3);

			// it is vital to have version information!
			// if we don't have any we assume boost 1.33
			if (flags & no_header)
				set_library_version(input_library_version);

			// extract and check the magic eos byte
			else if (load_signed_char() != magic_byte)
				throw archive_exception(archive_exception::invalid_signature);

			else
			{
				// extract version information
				operator>>(input_library_version);

				// throw if file version is newer than we are
				if (input_library_version > archive_version)
					throw archive_exception(archive_exception::unsupported_version);

				// else set the library version accordingly
				else set_library_version(input_library_version);
			}
		}

	public:
		/**
		 * \brief Constructor on a stream using ios::binary mode!
		 *
		 * We cannot call basic_binary_iprimitive::init which tries to detect
		 * if the binary archive stems from a different platform by examining
		 * type sizes.
		 *
		 * We could have called basic_binary_iarchive::init which would create
		 * the boost::serialization standard archive header containing also the
		 * library version. Due to efficiency we stick with our own.
		 */
		portable_iarchive(std::istream& is, unsigned flags = 0)
		#if BOOST_VERSION < 103400
			: portable_iprimitive(is, flags & boost::archive::no_codecvt)
		#else
			: portable_iprimitive(*is.rdbuf(), flags & boost::archive::no_codecvt)
		#endif
			, boost::archive::basic_binary_iarchive<portable_iarchive>(flags)
		{
			init(flags);
		}

	#if BOOST_VERSION >= 103400
		portable_iarchive(std::streambuf& sb, unsigned flags = 0)
			: portable_iprimitive(sb, flags & boost::archive::no_codecvt)
			, boost::archive::basic_binary_iarchive<portable_iarchive>(flags)
		{
			init(flags);
		}
	#endif

		//! Load narrow strings.
		void load(std::string& s) 
		{
			portable_iprimitive::load(s);
		}

	#ifndef BOOST_NO_STD_WSTRING
		/**
		 * \brief Load wide strings.
		 *
		 * This is rather tricky to get right for true portability as there
		 * are so many different character encodings around. However, wide
		 * strings that are encoded in one of the Unicode schemes only need
		 * to be _transcoded_ which is a lot easier actually.
		 *
		 * We generate the output string to be encoded in the system's native
		 * format, ie. UTF-16 on Windows and UTF-32 on Linux machines. Don't
		 * know about Mac here so I can't really say about that.
		 */
		void load(std::wstring& s)
		{
			std::string utf8;
			load(utf8);
			s = boost::from_utf8(utf8);
		}
	#endif

        /**
         * \brief Loading bool type.
         *
         * Byte pattern is same as with integer types, so this function
         * is somewhat redundant but treating bool as integer generates
		 * a lot of compiler warnings.
         *
         * \note If you cannot compile your application and it says something
         * about load(bool) cannot convert your type A& into bool& then you
         * should check your BOOST_CLASS_IMPLEMENTATION setting for A, as
         * portable_archive is not able to handle custom primitive types in
         * a general manner.
         */
		void load(bool& b) 
		{ 
			switch (signed char c = load_signed_char())
			{
			case 0: b = false; break;
			case 1: b = load_signed_char(); break;
			default: throw portable_archive_exception(c);
			}
		}

		/**
		 * \brief Load integer types.
		 *
		 * First we load the size information ie. the number of bytes that 
		 * hold the actual data. Then we retrieve the data and transform it
		 * to the original value by using load_little_endian.
		 */
		template <typename T>
		typename boost::enable_if<boost::is_integral<T> >::type
		load(T & t, dummy<2> = 0)
		{
			// get the number of bytes in the stream
			if (signed char size = load_signed_char())
			{
				// check for negative value in unsigned type
				if (size < 0 && boost::is_unsigned<T>::value)
					throw portable_archive_exception();

				// check that our type T is large enough
				else if ((unsigned) abs(size) > sizeof(T)) 
					throw portable_archive_exception(size);

				// reconstruct the value
				T temp = size < 0 ? -1 : 0;
				load_binary(&temp, abs(size));

				// load the value from little endian - it is then converted
				// to the target type T and fits it because size <= sizeof(T)
				t = endian::load_little_endian<T, sizeof(T)>(&temp);
			}

			else t = 0; // zero optimization
		}

		/** 
		 * \brief Load floating point types.
		 * 
		 * We simply rely on fp_traits to set the bit pattern from the (unsigned)
		 * integral type that was stored in the stream. Francois Mauger provided
		 * standardized behaviour for special values like inf and NaN, that need to
		 * be serialized in his application.
		 *
		 * \note by Johan Rade (author of the floating point utilities library):
		 * Be warned that the math::detail::fp_traits<T>::type::get_bits() function 
		 * is *not* guaranteed to give you all bits of the floating point number. It
		 * will give you all bits if and only if there is an integer type that has
		 * the same size as the floating point you are copying from. It will not
		 * give you all bits for double if there is no uint64_t. It will not give
		 * you all bits for long double if sizeof(long double) > 8 or there is no
		 * uint64_t. 
		 * 
		 * The member fp_traits<T>::type::coverage will tell you whether all bits
		 * are copied. This is a typedef for either math::detail::all_bits or
		 * math::detail::not_all_bits. 
		 * 
		 * If the function does not copy all bits, then it will copy the most
		 * significant bits. So if you serialize and deserialize the way you
		 * describe, and fp_traits<T>::type::coverage is math::detail::not_all_bits,
		 * then your floating point numbers will be truncated. This will introduce
		 * small rounding off errors. 
		 */
		template <typename T>
		typename boost::enable_if<boost::is_floating_point<T> >::type
		load(T & t, dummy<3> = 0)
		{
			typedef typename fp::detail::fp_traits<T>::type traits;

			// if you end here there are three possibilities:
			// 1. you're serializing a long double which is not portable
			// 2. you're serializing a double but have no 64 bit integer
			// 3. your machine is using an unknown floating point format
			// after reading the note above you still might decide to 
			// deactivate this static assert and try if it works out.
			typename traits::bits bits;
			BOOST_STATIC_ASSERT(sizeof(bits) == sizeof(T));
			BOOST_STATIC_ASSERT(std::numeric_limits<T>::is_iec559);

			load(bits);
			traits::set_bits(t, bits);

			// if the no_infnan flag is set we must throw here
			if (get_flags() & no_infnan && !fp::isfinite(t))
				throw portable_archive_exception(t);

			// if you end here your floating point type does not support 
			// denormalized numbers. this might be the case even though 
			// your type conforms to IEC 559 (and thus to IEEE 754)
			if (std::numeric_limits<T>::has_denorm == std::denorm_absent
				&& fp::fpclassify(t) == (int) FP_SUBNORMAL) // GCC4
				throw portable_archive_exception(t);
		}

		// in boost 1.44 version_type was splitted into library_version_type and
		// item_version_type, plus a whole bunch of additional strong typedefs.
		template <typename T>
		typename boost::disable_if<boost::is_arithmetic<T> >::type
		load(T& t, dummy<4> = 0)
		{
			// we provide a generic load routine for all types that feature
			// conversion operators into an unsigned integer value like those
			// created through BOOST_STRONG_TYPEDEF(X, some unsigned int) like
			// library_version_type, collection_size_type, item_version_type,
			// class_id_type, object_id_type, version_type and tracking_type
			load((typename boost::uint_t<sizeof(T)*CHAR_BIT>::least&)(t));
		}
	};

	// polymorphic portable binary iarchive typedef
	typedef POLYMORPHIC(portable_iarchive) polymorphic_portable_iarchive;
	#undef POLYMORPHIC

} // namespace eos

// this is required by export which registers all of your
// classes with all the inbuilt archives plus our archive.
#if BOOST_VERSION < 103500
#define BOOST_ARCHIVE_CUSTOM_IARCHIVE_TYPES eos::portable_iarchive
#else
BOOST_SERIALIZATION_REGISTER_ARCHIVE(eos::portable_iarchive)
BOOST_SERIALIZATION_REGISTER_ARCHIVE(eos::polymorphic_portable_iarchive)
#endif

// if you include this header multiple times and your compiler is picky
// about multiple template instantiations (eg. gcc is) then you need to
// define NO_EXPLICIT_TEMPLATE_INSTANTIATION before every include but one
// or you move the instantiation section into an implementation file
#ifndef NO_EXPLICIT_TEMPLATE_INSTANTIATION

#include <boost/archive/impl/basic_binary_iarchive.ipp>
#include <boost/archive/impl/basic_binary_iprimitive.ipp>

#if BOOST_VERSION < 104000
#include <boost/archive/impl/archive_pointer_iserializer.ipp>
#elif !defined BOOST_ARCHIVE_SERIALIZER_INCLUDED
#include <boost/archive/impl/archive_serializer_map.ipp>
#define BOOST_ARCHIVE_SERIALIZER_INCLUDED
#endif

namespace boost { namespace archive {

	// explicitly instantiate for this type of binary stream
	template class basic_binary_iarchive<eos::portable_iarchive>;

	template class basic_binary_iprimitive<
		eos::portable_iarchive
	#if BOOST_VERSION < 103400
		, std::istream
	#else
		, std::istream::char_type
		, std::istream::traits_type
	#endif
	>;

#if BOOST_VERSION < 104000
	template class detail::archive_pointer_iserializer<eos::portable_iarchive>;
#else
	template class detail::archive_serializer_map<eos::portable_iarchive>;
	//template class detail::archive_serializer_map<eos::polymorphic_portable_iarchive>;
#endif

} } // namespace boost::archive

#endif
