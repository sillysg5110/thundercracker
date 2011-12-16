/* -*- mode: C; c-basic-offset: 4; intent-tabs-mode: nil -*-
 *
 * STIR -- Sifteo Tiled Image Reducer
 * M. Elizabeth Scott <beth@sifteo.com>
 *
 * Copyright <c> 2011 Sifteo, Inc. All rights reserved.
 */

#include "cppwriter.h"

namespace Stir {

const char *CPPWriter::indent = "    ";

CPPWriter::CPPWriter(Logger &log, const char *filename)
    : mLog(log)
{
    if (filename) {
        mStream.open(filename);
        if (!mStream.is_open())
            log.error("Error opening output file '%s'", filename);
    }
    
    if (mStream.is_open())
        head();
}

void CPPWriter::head()
{
    mStream <<
        "/*\n"
        " * Generated by STIR. Do not edit by hand.\n"
        " */\n"
        "\n"
        "#include <sifteo/asset.h>\n";
}

void CPPWriter::foot()
{}
 
void CPPWriter::close()
{
    if (mStream.is_open()) {
        foot();
        mStream.close();
    }
}

void CPPWriter::writeArray(const std::vector<uint8_t> &array)
{
    char buf[8];

    mStream << indent;

    for (unsigned i = 0; i < array.size(); i++) {
        if (i && !(i % 16))
            mStream << "\n" << indent;

        sprintf(buf, "0x%02x,", array[i]);
        mStream << buf;
    }

    mStream << "\n";
}


CPPSourceWriter::CPPSourceWriter(Logger &log, const char *filename)
    : CPPWriter(log, filename)
    {}

void CPPSourceWriter::writeGroup(const Group &group)
{
    char buf[32];

    #ifdef __MINGW32__
    sprintf(buf, "0x%016I64x", (long long unsigned int) group.getSignature());
    #else
    sprintf(buf, "0x%016llx", (long long unsigned int) group.getSignature());
    #endif

    // TODO: increment ids correctly with each group
    uint32_t id = 1;
    
    mStream << "\nuint32_t " << group.getName() << "ID_int = " << id << ";\n";
    mStream <<
        "Sifteo::AssetGroupID " << group.getName() << "ID"
        " = {{ " << group.getName() << "ID_int, (uint32_t)0, (uint32_t)0," << group.getName() << "ID.cubes }};\n";

    mStream <<
        "\n"
        "static const struct {\n" <<
        indent << "struct _SYSAssetGroupHeader hdr;\n" <<
        indent << "uint8_t data[" << group.getLoadstream().size() << "];\n"
        "} " << group.getName() << "_data = {{\n" <<
        indent << "/* hdrSize   */ sizeof(struct _SYSAssetGroupHeader),\n" <<
        indent << "/* reserved  */ 0,\n" <<
        indent << "/* numTiles  */ " << group.getPool().size() << ",\n" <<
        indent << "/* dataSize  */ " << group.getLoadstream().size() << ",\n" <<
        indent << "/* signature */ " << buf << ",\n"
        "}, {\n";

    writeArray(group.getLoadstream());

    mStream <<
        "}};\n\n"
        "Sifteo::AssetGroup " << group.getName() <<
        " = {{ &" << group.getName() << "_data.hdr, " << group.getName() << ".cubes }};\n";

    for (std::set<Image*>::iterator i = group.getImages().begin();
         i != group.getImages().end(); i++)
        writeImage(**i);
}

void CPPSourceWriter::writeSound(const Sound &sound)
{
    // TODO: increment ids correctly with each group
    uint32_t id = 2;
    
    mStream <<
        "_SYSAudioModuleID " << sound.getName() << "= {\n" <<
        id << ",\n" <<
        "0,\n" <<
        "0,\n" <<
        "Sample\n" <<
        "};\n";
}

void CPPSourceWriter::writeImage(const Image &image)
{
    char buf[16];
    const std::vector<TileGrid> &grids = image.getGrids();
    unsigned width = grids.empty() ? 0 : grids[0].width();
    unsigned height = grids.empty() ? 0 : grids[0].height();

    if (image.isPinned()) {
        /*
         * Sifteo::PinnedAssetImage
         */
        
        const TileGrid &grid = grids[0];
        const TilePool &pool = grid.getPool();

        mStream <<
            "\n"
            "Sifteo::PinnedAssetImage " << image.getName() << " = {\n" <<
            indent << "/* width   */ " << width << ",\n" <<
            indent << "/* height  */ " << height << ",\n" <<
            indent << "/* frames  */ " << grids.size() << ",\n" <<
            indent << "/* index   */ " << pool.index(grid.tile(0, 0)) <<
            ",\n};\n";
            
    } else {
        /*
         * Sifteo::AssetImage
         *
         * Write out an uncompressed tile grid.
         *
         * XXX: Compression!
         */

        mStream << "\nstatic const uint16_t " << image.getName() << "_tiles[] = {\n";

        for (unsigned f = 0; f < grids.size(); f++) {
            const TileGrid &grid = grids[f];
            const TilePool &pool = grid.getPool();

            mStream << indent << "// Frame " << f << "\n";
        
            for (unsigned y = 0; y < height; y++) {
                mStream << indent;
                for (unsigned x = 0; x < width; x++) {
                    sprintf(buf, "0x%04x,", pool.index(grid.tile(x, y)));
                    mStream << buf;
                }
                mStream << "\n";
            }
        }
        
        mStream <<
            "};\n\n"
            "Sifteo::AssetImage " << image.getName() << " = {\n" <<
            indent << "/* width   */ " << width << ",\n" <<
            indent << "/* height  */ " << height << ",\n" <<
            indent << "/* frames  */ " << grids.size() << ",\n" <<
            indent << "/* tiles   */ " << image.getName() << "_tiles,\n" <<
            "};\n";
    }
}

CPPHeaderWriter::CPPHeaderWriter(Logger &log, const char *filename)
    : CPPWriter(log, filename)
{
    if (filename)
        createGuardName(filename);

    if (mStream.is_open())
        head();
}

void CPPHeaderWriter::createGuardName(const char *filename)
{
    /*
     * Make a name for the include guard, based on the filename
     */

    char c;
    char prev = '_';
    guardName = prev;

    while ((c = *filename)) {
        c = toupper(c);

        if (isalpha(c)) {
            prev = c;
            guardName += prev;
        } else if (prev != '_') {
            prev = '_';
            guardName += prev;
        }

        filename++;
    }
}

void CPPHeaderWriter::head()
{
    mStream <<
        "\n"
        "#ifndef " << guardName << "\n"
        "#define " << guardName << "\n"
        "\n";
}

void CPPHeaderWriter::foot()
{
    mStream <<
        "\n"
        "#endif  // " << guardName << "\n";

    CPPWriter::foot();
}

void CPPHeaderWriter::writeGroup(const Group &group)
{
    mStream << "extern Sifteo::AssetGroupID " << group.getName() << "ID;\n";
    
    mStream << "extern Sifteo::AssetGroup " << group.getName() << ";\n";

    for (std::set<Image*>::iterator i = group.getImages().begin();
         i != group.getImages().end(); i++) {
        Image *image = *i;
        const char *cls;
        
        if (image->isPinned())
            cls = "PinnedAssetImage";
        else
            cls = "AssetImage";
            
        mStream << "extern const Sifteo::" << cls << " " << image->getName() << ";\n";
    }
}

void CPPHeaderWriter::writeSound(const Sound &sound)
{
    mStream << "extern _SYSAudioModuleID " << sound.getName() << ";\n";
}

};  // namespace Stir
