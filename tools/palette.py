#! /usr/bin/env python3
import argparse
import io
import os
import os.path
import sys
import struct
import textwrap
from PIL import Image  # type: ignore
from enum import Enum

# palette.py
# Tool converts a palette file of supported type into a .c file that defines the palette as a uint32_t array.
# This can then be memcpy'd into a TA palette bank directly

PaletteType = Enum('PaletteType', ['Unknown', 'GPL', 'PAL', 'PNG', 'ASE', 'ACO', 'TXT'])

def is_type_plaintext(input: PaletteType) -> bool:
    if input == PaletteType.GPL or input == PaletteType.PAL or input == PaletteType.TXT:
        return True
    return False

def is_type_printable_bin(input: PaletteType) -> bool:
    if input == PaletteType.ASE or input == PaletteType.ACO:
        return True
    return False

# Class to wrap palette data to be parsed
class UnparsedPaletteData:
    palette_type = PaletteType.Unknown
    args = None

    def __init__(self,ar):
        self.args = ar
        self.determine_type()

    # Automatically determine the type of file, unless a specific type has been specified.
    def determine_type(self):
        file_name, file_extension = os.path.splitext(self.args.palette)
        file_extension = file_extension.strip(",.-_")
        for pt in PaletteType:
            # If a type has been specified, use that
            if self.args.type != 'Unknown':
                if self.args.type.lower() == pt.name.lower():
                    self.palette_type = pt
                    # self.args.force = True
                    break
            # Otherwise, try to match the filename against the supported types enum
            else:
                if file_extension.lower() == pt.name.lower():
                    self.palette_type = pt
                    break

        if self.palette_type == PaletteType.Unknown:
            raise Exception("ERROR: Could not determine palette type. Please check filename and extension, and/or specify a --type flag.")

        return

# Class to return parsed palette data
# Not all file types will populate all fields!
class ParsedPaletteData:
    palette_type = PaletteType.Unknown
    palette_colors = []
    palette_name = ""
    color_count = -1
    raw_source = ""

    # Print the parsed palette data (EG: for debug print etc.)
    def __str__(self):
        nl = '\n'
        strpc = [str(pc) for pc in self.palette_colors]
        return f"Name: {self.palette_name}{nl}Colors: {self.color_count}{nl}{nl.join(strpc)}"

# Parse a .GPL or .PAL file and return colors in the form of separated r,g,b values.
# Add alpha value based on the specified transparent index (or default index 0).
def parse_gpl_pal(input: UnparsedPaletteData) -> ParsedPaletteData:
    output = ParsedPaletteData()
    output.palette_type = input.palette_type

    with open(input.args.palette, "rt") as f:
        lines = f.readlines()

        for index, line in enumerate(lines, start=0):
            line = line.strip()
            
            # Make sure this is a valid GPL or PAL format file
            if index == 0:
                if line.lower().find("gimp palette") == 0:
                    output.palette_type = PaletteType.GPL
                    continue
                elif line.lower().find("jasc-pal") == 0:
                    output.palette_type = PaletteType.PAL
                    continue
                elif input.args.force == False:
                    raise Exception(f"ERROR: Invalid or malformed palette file passed to palette.py! {input.args.palette}")

            # Parse GPL
            if output.palette_type == PaletteType.GPL:
                # Name may not be included
                if line.lower().find("name:") != -1:
                    nameSplit = line.split(":")
                    output.palette_name = nameSplit[-1]
                    continue
                # Color count is common, but not guaranteed as part of spec
                if line.lower().find("colors:") != -1:
                    color_countSplit = line.split(":")
                    output.color_count = int(color_countSplit[-1])
                    continue

                # Ignore other commented lines
                if line.find("#") != -1:
                    continue
            
            # Parse PAL
            else:
                if index == 1:
                    continue
                # Color count is part of spec
                if index == 2:
                    output.color_count = int(line)
                    continue
            
            columns = line.split()
            if len(columns) >= 3:
                # GPL and PAL files do not support transparency, so define alpha 255 or 0
                # based on the '--alpha' argument
                a = 255
                if len(output.palette_colors) == input.args.alpha:
                    a = 0
                output.palette_colors.append([int(columns[0]), int(columns[1]), int(columns[2]), a])

    if output.color_count != -1 and len(output.palette_colors) != output.color_count and input.args.force == False:
        raise Exception(f"ERROR: Parsed color count does not match file declaration! {len(output.palette_colors)} != {output.color_count}")
    elif output.color_count == -1:
        output.color_count = len(output.palette_colors)

    # Validate color count is either 16 or 256
    if output.color_count != 16 and output.color_count != 256:
        raise Exception(f"ERROR: Parsed color count does not align with supported NAOMI palette sizes! {output.color_count} != 16 or 256")

    output.raw_source = "".join(lines)

    return output

# Parse a .TXT palette file and return colors in the form of r,g,b,a values.
# Format spec seems to be: https://www.getpaint.net/doc/latest/WorkingWithPalettes.html
def parse_txt(input: UnparsedPaletteData) -> ParsedPaletteData:
    output = ParsedPaletteData()
    output.palette_type = input.palette_type

    with open(input.args.palette, "rt") as f:
        lines = f.readlines()

        for index, line in enumerate(lines, start=0):
            line = line.strip()
            
            # Make sure this is a valid Paint.net TXT format file
            if index == 0:
                if line.lower().find("paint.net palette file") != -1:
                    continue
                elif input.args.force == False:
                    raise Exception(f"ERROR: Invalid or malformed TXT palette file passed to palette.py! {input.args.palette}")

            # Parse TXT format
            # Name is not part of spec, but may be included
            if line.lower().find("name:") != -1:
                nameSplit = line.split(":")
                output.palette_name = nameSplit[-1]
                continue

            # Color count is not part of spec, but may be included
            if line.lower().find("colors:") != -1:
                color_countSplit = line.split(":")
                output.color_count = int(color_countSplit[-1])
                continue

            # Ignore other commented lines
            if line.find(";") != -1:
                continue
            
            # Colors are listed in hex format aarrggbb
            if len(line) >= 8:
                output.palette_colors.append([int(line[2:4],16), int(line[4:6],16), int(line[6:8],16), int(line[0:2],16)])
    
    if output.color_count == -1:
        output.color_count = len(output.palette_colors)

    output.raw_source = "".join(lines)

    return output

# Parse a .PNG file and return return colors in the form of r,g,b,a values.
# Processes the first 16 or 256 pixels of the PNG.
def parse_png(input: UnparsedPaletteData) -> ParsedPaletteData:
    output = ParsedPaletteData()
    output.palette_type = input.palette_type

    # Open image, get pixels array, convert to RGBA
    with open(input.args.palette, "rb") as bfp:
        data = bfp.read()
    texture = Image.open(io.BytesIO(data))
    width, height = texture.size

    # Determine the number of pixels to encode as a palette
    # Automatically take the larger possible size if possible
    pix_count = width * height
    if pix_count < 16:
        raise Exception(f"ERROR: PNG pixel count is smaller than minimum NAOMI palette size! {width * height} < 16")
    elif pix_count >= 16 and pix_count < 256:
        pix_count = 16
    else:
        pix_count = 256

    output.color_count = pix_count

    pixels = texture.convert('RGBA')
    for index, pixdata in enumerate(pixels.getdata(), start=0):
        if index >= pix_count:
            break
        # pixdata is [r, g, b, a]
        output.palette_colors.append([pixdata[0], pixdata[1], pixdata[2], pixdata[3]])

    return output


# Append a color to the palette_colors list and address alpha if needed
# Used by both parse_ase() and parse_aco()
def append_col_alpha_index(input: UnparsedPaletteData, output: ParsedPaletteData, col):
    # Initialize the output color count if it's still in the "undefined" state
    if output.color_count == -1:
        output.color_count = 0
    
    output.color_count += 1

    a = 255
    if len(output.palette_colors) == input.args.alpha:
        a = 0

    outcol = (col[0], col[1], col[2], a)
    output.palette_colors.append(outcol)

# Parse a .ASE file and return colors in the form of separated r,g,b values.
# Add alpha value based on the specified transparent index (or default index 0).

# As currently designed, this will only import the first 16 or 256 colors in the
# ASE file, and will ignore any ASE "groups" and color names.

# Note that ASE is a proprietary file format and a public spec is not available;
# support in this tool is based on the reverse-engineering efforts of others.

# References used to understand the file format:
# https://carl.camera/default.aspx?id=109
# http://www.selapa.net/swatches/colors/fileformats.php#adobe_ase
# https://www.cyotek.com/blog/reading-adobe-swatch-exchange-ase-files-using-csharp
def parse_ase(input: UnparsedPaletteData) -> ParsedPaletteData:
    output = ParsedPaletteData()
    output.palette_type = input.palette_type

    fsize = os.stat(input.args.palette).st_size

    with open(input.args.palette, 'rb') as asebin:
        ase = asebin.read()

        # ASE files are big-endian encoded, so use struct unpack to 
        # ensure endianness is correctly interpreted
        # First 12 bytes is header
        header = struct.unpack('>4c2h1I', ase[0:12])

        asef_signature = b''.join(header[0:4]).decode('ascii')
        ver = f"{int(header[4])}.{int(header[5])}"        
        chunk_count = int(header[6])

        if asef_signature.lower() != 'asef' or chunk_count <= 0:
            raise Exception(f"ERROR: Invalid or malformed .ase file passed to palette.py! {input.args.palette}")

        offset = 12

        parsed_src = []
        parsed_src.append(f"Header: {asef_signature} {ver} {chunk_count}")

        ase_color_types = ("Global", "Spot", "Normal")

        while offset < fsize:
            # Get header data for this chunk
            chunk_header = struct.unpack('>HIH', ase[offset:offset+8])
            offset += 6
            ch_offset = offset + 2

            ch_type = chunk_header[0]
            ch_size = int(chunk_header[1])
            ch_name_length = int(chunk_header[2])

            # Handle different chunk types
            if ch_type == 0xC001:
                ch_name_raw = struct.unpack(f">{ch_name_length}H", ase[ch_offset:ch_offset + (ch_name_length*2)])
                ch_name = ''.join(chr(n) for n in ch_name_raw).strip().rstrip('\x00')
                ch_offset += (ch_name_length*2)

                # print(f"GROUP START ({ch_size}B) Name ({ch_name_length}B): {ch_name}")
                parsed_src.append(f"GROUP START ({ch_size}B) Name ({ch_name_length}B): {ch_name}")
                
                offset += ch_size

            elif ch_type == 0x0001:
                # Parse the name block for this color
                # It's null-terminated, so the name_length will report 1 extra character
                ch_name_raw = struct.unpack(f">{ch_name_length}H", ase[ch_offset:ch_offset + (ch_name_length*2)])
                ch_name = ''.join(chr(n) for n in ch_name_raw).strip().rstrip('\x00')
                ch_offset += (ch_name_length*2)

                # Color mode
                color_mode_raw = struct.unpack('>4c', ase[ch_offset:ch_offset+4])
                color_mode = b''.join(color_mode_raw).decode('ascii')
                ch_offset += 4

                # The actual color data, finally
                # Based on color mode, determine the number of color values to parse

                # Currently only RGB and Grayscale color modes are supported.
                # CMYK and LAB conversions are theoretically possible, but rely on
                # ICC profiles and other reference information that makes them
                # more complicated to manage without introducing conversion errors
                if color_mode.lower().strip() == 'rgb':
                    col_val = struct.unpack('>3f', ase[ch_offset:ch_offset+12])
                    ch_offset += 12
                    rgb_col = (int(col_val[0] * 255), int(col_val[1] * 255), int(col_val[2] * 255))
                    append_col_alpha_index(input, output, rgb_col)

                elif color_mode.lower().strip() == 'cmyk':
                    col_val = struct.unpack('>4f', ase[ch_offset:ch_offset+16])
                    ch_offset += 16
                    raise Exception(f"ERROR: Unsupported Color Mode: {color_mode}")

                elif color_mode.lower().strip() == 'lab':
                    col_val = struct.unpack('>3f', ase[ch_offset:ch_offset+12])
                    ch_offset += 12
                    raise Exception(f"ERROR: Unsupported Color Mode: {color_mode}")

                elif color_mode.lower().strip() == 'gray':
                    col_val = struct.unpack('>f', ase[ch_offset:ch_offset+4])
                    ch_offset += 4
                    rgb_col = (int(col_val * 255), int(col_val * 255), int(col_val * 255))
                    append_col_alpha_index(input, output, rgb_col)

                else:
                    raise Exception(f"ERROR: Unknown Color Mode: {color_mode}")

                # Type flag for the color (0 = Global, 1 = Spot, 2 = Normal)
                # Not actually utilized by NAOMI palettes
                col_type = struct.unpack('>H', ase[ch_offset:ch_offset+2])

                parsed_src.append(f"COLOR ({ch_size}B) Name ({ch_name_length}B): {ch_name} Mode: {color_mode} Val: {col_val} Type: {ase_color_types[col_type[0]]}")
                offset += ch_size

            elif ch_type == 0x0000:
                # print(f"GROUP END Size: {ch_size}")
                parsed_src.append(f"GROUP END ({ch_size}B)")
                
                offset += ch_size
                break
            else:
                raise Exception(f"ERROR: Invalid or malformed .ase file passed to palette.py! Malformed chunk header. {input.args.palette}")
        
        output.raw_source = '\n'.join(parsed_src)

    return output

# Parse a .ACO file and return colors in the form of separated r,g,b values.
# Add alpha value based on the specified transparent index (or default index 0).

# ACO spec was published by Adobe: 
# https://www.adobe.com/devnet-apps/photoshop/fileformatashtml/#50577411_pgfId-1055819
# Some details, such as custom color space support, are omitted from the spec, but they
# are not relevant to this use case.

# Version 1 is nominally required by spec, but anecdotal evidence suggests it can
# sometimes be omitted from the file, so prefer Version 2 format when it exists
def parse_aco(input: UnparsedPaletteData) -> ParsedPaletteData:
    output = ParsedPaletteData()
    output.palette_type = input.palette_type

    fsize = os.stat(input.args.palette).st_size

    with open(input.args.palette, 'rb') as acobin:
        aco = acobin.read()

        offset = 0

        # ACO files are big-endian encoded, so use struct unpack to 
        # ensure endianness is correctly interpreted.
        # First bytes should be the Version header and color count
        ver = struct.unpack('>H', aco[offset:offset+2])[0]
        offset += 2

        if ver != 1 and ver != 2:
            raise Exception(f"ERROR: Invalid ACO file version found. Expected 1 or 2, found {ver}")

        col_count = struct.unpack('>H', aco[offset:offset+2])[0]
        offset += 2

        if ver != 1 and ver != 2:
            raise Exception(f"ERROR: ACO file does not align with supported NAOMI palette sizes! {col_count} != 16 or 256")

        # If we found a V1 header, try to advance to the expected V2 data location automatically
        if ver == 1:
            # Jump ahead to the expected V2 header location, if it exists
            t_offset = offset + (col_count * 10)
            if t_offset < fsize:
                t_ver = struct.unpack('>H', aco[t_offset:t_offset+2])[0]
                t_offset += 2

                # If we find a valid V2 header, advance the offset to it
                if t_ver == 2:
                    offset = t_offset + 2
                    ver = 2
        

        parsed_src = []
        parsed_src.append(f"Header: V{ver} {col_count}")

        # Iterate through color entries, with slightly adjusted parsing based on Version.
        while offset <= fsize:
            color_mode = struct.unpack('>H', aco[offset:offset+2])[0]
            color_val = struct.unpack('>HHHH', aco[offset+2:offset+10])
            offset += 10

            # Currently only RGB and Grayscale color modes are supported.
            # HSB, CMYK, and LAB conversions are theoretically possible, but rely
            # on ICC profiles and other reference information that make them
            # more complicated to manage without introducing conversion errors

            # RGB Mode
            if color_mode == 0:
                rgb_col = (int(color_val[0] / 256), int(color_val[1] / 256), int(color_val[2] / 256))
                append_col_alpha_index(input, output, rgb_col)
            # HSB Mode
            elif color_mode == 1:
                raise Exception(f"ERROR: Unsupported Color Mode: HSB")
            # CMYK
            elif color_mode == 2:
                raise Exception(f"ERROR: Unsupported Color Mode: CMYK")
            # LAB 
            elif color_mode == 7:
                raise Exception(f"ERROR: Unsupported Color Mode: LAB")
            # Grayscale
            elif color_mode == 8:
                # Grayscale is encoded in 0...10000 range, so uses an unusual division
                # to convert to 0...255 http://www.nomodes.com/aco.html
                gray_val = int(color_val[0] / 39.0625)
                rgb_col = (gray_val, gray_val, gray_val)
                append_col_alpha_index(input, output, rgb_col)

            # Version 2 files include color names which must be parsed
            ch_name = ''
            if ver == 2:
                ch_name_length = struct.unpack('>I', aco[offset:offset+4])[0]
                offset += 4
                ch_name_raw = struct.unpack(f">{ch_name_length}H", aco[offset:offset + (ch_name_length*2)])
                ch_name = ''.join(chr(n) for n in ch_name_raw).strip()
                offset += (ch_name_length*2)

            parsed_src.append(f"Raw: {color_mode} {color_val} -> Parsed RGB: {rgb_col} {ch_name}")

            # If we've parsed the entire defined color array, exit without parsing V2 data
            if output.color_count == col_count:
                break

        output.raw_source = '\n'.join(parsed_src)

    return output


# ======================================================================================================

# Converts a palette file of supported type into a .c file that defines the palette as a uint32_t array.
# This can then be memcpy'd into a TA palette bank directly
def main() -> int:
    parser = argparse.ArgumentParser(
        description=(
            "Utility for converting various palette files to C include style."
            "Supported types include .gpl and .pal palette formats, .png files, .ase and .aco swatches, and Paint.NET .txt palettes."
        )
    )
    parser.add_argument(
        'file',
        metavar='FILE',
        type=str,
        help='The output file we should generate.',
    )
    parser.add_argument(
        'palette',
        metavar='PALETTE',
        type=str,
        help='The palette file we should use to generate the output file.',
    )
    parser.add_argument(
        '-m', '--mode',
        metavar='MODE',
        type=str,
        help=(
            'The mode of the palette data to construct. Should match the video mode you are initializing.'
            'Options are "RGBA1555" and "RGBA8888".'
        ),
    )
    parser.add_argument(
        '-r', '--raw',
        action="store_true",
        help='Output a raw palette binary file instead of a C include file.',
    )
    parser.add_argument(
        '-t', '--type',
        nargs='?',
        const='Unknown',
        default='Unknown',
        metavar='TYPE',
        type=str,
        help=(
            'An optional parameter to try parsing as if the target is a specified file type.'
            'Options are "GPL", "PAL", "PNG", "ASE", "ACO", and "TXT".'
            'Normally, palette.py will automatically determine the file type based on extension and contents.'
            'However, setting this flag allows that behavior to be manually overridden.'
        )
    )
    parser.add_argument(
        '-f', '--force',
        nargs='?',
        const=False,
        default=False,
        metavar='FORCE',
        type=bool,
        help=(
            'An optional parameter to force parsing, ignoring some sanity and type checks.'
            'Intended for special use cases, generally in combination with the --type parameter.'
        )
    )
    parser.add_argument(
        '-a', '--alpha',
        nargs='?',
        const=0,
        default=0,
        metavar='ALPHA',
        type=int,
        help=( 
            'The index of the transparent color in the palette. GPL, PAL, ASE, and ACO palettes do not record transparency,'
            'so this must be passed as an option when converting. Default is 0 if not specified.'
            'A value < 0 or > palette color count will result in fully opaque output.'
        )
    )
    args = parser.parse_args()

    # Construct the raw data object and try to automatically determine the palette type
    raw_data = UnparsedPaletteData(args)

    if raw_data.palette_type == PaletteType.GPL or raw_data.palette_type == PaletteType.PAL:
        palette_data = parse_gpl_pal(raw_data)
    elif raw_data.palette_type == PaletteType.PNG:
        palette_data = parse_png(raw_data)
    elif raw_data.palette_type == PaletteType.TXT:
        palette_data = parse_txt(raw_data)
    elif raw_data.palette_type == PaletteType.ASE:
        palette_data = parse_ase(raw_data)
    elif raw_data.palette_type == PaletteType.ACO:
        palette_data = parse_aco(raw_data)
    else:
        raise Exception(f"ERROR: Unrecognized file type. Please check file extension and/or specify a --type to parse.")

    # If parsed color count is -1, there was an unhandled parse error
    if palette_data.color_count == -1:
        raise Exception(f"ERROR: Parsing error when trying to process file {args.palette}")

    # Validate color count is either 16 or 256
    if palette_data.color_count != 16 and palette_data.color_count != 256:
        raise Exception(f"ERROR: Parsed color count does not align with supported NAOMI palette sizes! {palette_data.color_count} != 16 or 256")

    # DEBUG: Print palette info
    # print(palette_data)

    # Convert the data to uint32 format, following the RGB1555 or RGB8888 macros:
    # #define RGB1555(r, g, b, a) ((((b) >> 3) & (0x1F << 0)) | (((g) << 2) & (0x1F << 5)) | (((r) << 7) & (0x1F << 10)) | (((a) << 8) & 0x8000))
    # #define RGB0888(r, g, b) (((b) & 0xFF) | (((g) << 8) & 0xFF00) | (((r) << 16) & 0xFF0000) | 0xFF000000)
    out_colors = []
    mode = args.mode.lower()
    
    for pc in palette_data.palette_colors:
        if mode == "rgba1555": 
            out_colors.append( (((pc[2] >> 3) & (0x1F << 0)) | ((pc[1] << 2) & (0x1F << 5)) | ((pc[0] << 7) & (0x1F << 10)) | ((pc[3] << 8) & 0x8000)) & 0xFFFFFFFF)
        elif mode == "rgba8888":
            out_colors.append( (((pc[2] & 0xFF) << 0) | ((pc[1] & 0xFF) << 8) | ((pc[0] & 0xFF) << 16) | ((pc[3] & 0xFF) << 24)) & 0xFFFFFFFF)
        else:
            raise Exception(f"Unsupported depth {args.mode}!")
        
    # Write binary output if raw mode is enabled
    if args.raw:
        bin_colors = []
        for oc in out_colors:
            bin_colors.append(struct.pack("<I", oc))
        
        bindata = b"".join(bin_colors)
        with open(args.file, "wb") as bfp:
            bfp.write(bindata)
    else:
        name = os.path.basename(args.palette).replace('.', '_').replace('-', '_').replace(' ', '_')

        outstr = [str(oc) for oc in out_colors]

        # Include the source data (or a filename for binaries) in the generated c output file
        src = ""
        if is_type_plaintext(palette_data.palette_type):
            src = f"""// Original {palette_data.palette_type.name} file:
/*
{palette_data.raw_source}
*/
            """
        elif is_type_printable_bin(palette_data.palette_type):
            src = f"""// Source file:
// {args.palette}
// Interpreted binary block data:
/*
{palette_data.raw_source}
*/
            """
        else:
            src = f"""// Source file:
// {args.palette}
            """

        # Construct the output file
        cfile = f"""#include <stdint.h>

uint32_t __{name}_palette[{len(outstr)}] = {{
    {", ".join(outstr)}
}};
uint32_t *{name}_palette = __{name}_palette;

{src}
    """

        with open(args.file, "w") as sfp:
            sfp.write(textwrap.dedent(cfile))

    return 0


if __name__ == "__main__":
    sys.exit(main())