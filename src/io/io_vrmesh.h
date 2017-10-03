//
// Copyright (c) 2015-2017, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#ifndef VRAY_FOR_HOUDINI_IO_VRMESH_H
#define VRAY_FOR_HOUDINI_IO_VRMESH_H

#include "vfh_vray.h"

#include <GEO/GEO_IOTranslator.h>

namespace VRayForHoudini {
namespace IO {

/// Translator from/to Houdini geometry for .vrmesh files.
/// This allows .vrmesh files containing single frame data
/// to be read/saved directly from 'File' SOP.
/// For more info see:
/// http://archive.sidefx.com/docs/hdk15.5/_h_d_k__geometry__g_e_o_i_o_translator.html
class Vrmesh
		: public GEO_IOTranslator
{
public:
	static const char *const extension;

	Vrmesh() {}
	Vrmesh(const Vrmesh &other) {}

	virtual ~Vrmesh() {}

	/// Create a copy of the sub-class
	virtual GEO_IOTranslator * duplicate() const VRAY_OVERRIDE;

	/// Return the label for the geometry format that this translator supports.
	virtual const char * formatName() const VRAY_OVERRIDE;

	/// Check the extension of the name to see if it matches one that we can handle.
	/// @param name[in] - extension to check for
	/// @retval true if there's a match, false otherwise
	virtual int checkExtension(const char *name) VRAY_OVERRIDE;

	/// Check if the given magic number matches the magic number.
	/// @param magic[in] - number to check for
	/// @retval true if there's a match, false otherwise
	virtual int checkMagicNumber(unsigned magic) VRAY_OVERRIDE;

	/// Load a gdp from a stream. If the file format doesn't support reading from streams, it
	/// can use UT_IStream::isRandomAccessFile to get the raw name to read from.
	/// @param geo[out] - destination geometry detail to which new geometry will be added
	/// @param stream[in] - stream to read from
	/// @param ate_magic[in] - if the ate_magic flag is on, then the
	///	library has already read the magic number and the loader should
	///	not expect it.
	/// @retval false if this translator doesn't support loading.
	virtual GA_Detail::IOStatus fileLoad(GEO_Detail *geo, UT_IStream &stream,
										  bool ate_magic) VRAY_OVERRIDE;

	/// Save a gdp to another format via a filename. Return false if
	/// this translator does not support saving.
	/// @param geo[in] - source geometry detail to be saved
	/// @param stream[out] - stream to output to
	/// @retval currently this is not implemented and always returns false
	virtual GA_Detail::IOStatus fileSave(const GEO_Detail *geo, std::ostream &stream) VRAY_OVERRIDE;
};

} // namespace IO
} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_IO_VRMESH_H
