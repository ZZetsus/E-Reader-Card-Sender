import os
import struct
import subprocess

from include_py.crc32_table import crc32Table_4bytes

GAMEID = 0x47433645
CHALLENGE_4BYTE_SEED = 0x00000000

cards_metadata = []

BASE_PATH = os.path.dirname(os.path.abspath(__file__))

def crc32(CRC, chunk4byte):
    
    for i in range(4):

        byte = chunk4byte[i]
        index = (CRC ^ byte) & 0xff
        CRC = (CRC >> 8) ^ crc32Table_4bytes[index]
        
    return CRC

def challenge_4byte ():
    SEED_CANDIDATE = (GAMEID >> 8) ^ crc32Table_4bytes[(GAMEID ^ CHALLENGE_4BYTE_SEED) & 0xFF]
    REVERSED_SEED_CANDIDATE = ((SEED_CANDIDATE << 0x18) & 0xFFFFFFFF) | (((SEED_CANDIDATE & 0xFF00) << 8) & 0xFFFFFFFF) | ((SEED_CANDIDATE & 0xFFFFFFFF) >> 0x18) | ((SEED_CANDIDATE & 0xFF0000) >> 8)
    return REVERSED_SEED_CANDIDATE

def convert_bins_to_cpp():

    
    input_folder = os.path.join(BASE_PATH, 'e-cards', 'bin')
    output_file = os.path.join(BASE_PATH, 'source', 'carde_data.c')
    
    # There is an offset of 0x51 from the first byte of the bin.
    OFFSET = 0x51

    if not os.path.exists(input_folder):
        print(f"Error: Folder '{input_folder}' does not exist")
        return

    bin_files = [f for f in os.listdir(input_folder) if f.endswith('.bin')]
    
    if not bin_files:
        print("No .bin files found in /bin")
        return

    with open(output_file, 'w') as out:
        out.write("#include \"carde_data.h\" \n\n")

        card_meta_data = ""
        identifier_index = 0
        for bin_file in bin_files:
            file_path = os.path.join(input_folder, bin_file)
            var_name = os.path.splitext(bin_file)[0].replace(' ', '_').replace('-', '_')
            
            if len(var_name) > 20:
                var_name = var_name[:20]

            if os.path.exists(file_path):
                with open(file_path, 'rb') as f:
                    f.seek(OFFSET)
                    data = f.read()

            if len(data) % 4 != 0:
                data += b'\x00' * (4 - (len(data) % 4))

            real_elements = len(data) // 4
            total_elements = 1038 * 4

            out.write(f"//{bin_file} (Offset 0x51, {real_elements} data bin + CRC + challenge byte)\n")
            out.write(f"const uint32_t {var_name}[{real_elements}] = {{\n")
            
            CRC = GAMEID
            
            for i in range(0, total_elements , 4):

                if i < len(data):    

                    chunk = data[i:i+4]

                    val = struct.unpack('>I', chunk)[0]

                    out.write(f"    0x{val:08X},")
                    
                    if (i // 4 + 1) % 4 == 0:
                        out.write("\n")
                    else:
                        out.write(" ")

                    CRC = crc32(CRC, chunk)
                else: 
                    # print(f"i {i}\n")
    
                    chunk = b'\x00\x00\x00\x00'
                    CRC = crc32(CRC, chunk)

            out.write("};\n\n")        

            reversed_crc = struct.unpack('<I', struct.pack('>I', CRC))[0]

            
            # out.write(f"const uint32_t crc{var_name} = 0x{reversed_crc:08X}\n")

            # out.write(f"const uint8_t {var_name}_index = {identifier_index};\n")

            # out.write(f"const char {var_name}_label[30] = \"{var_name}\";\n\n")

            card_meta_data += f'    {{ {var_name}, "{var_name}", 0x{reversed_crc:08X}, {identifier_index}, {real_elements}}},\n' 
            identifier_index += 1

        out.write("const CardEntry card_list[] = {\n")
        out.write("// array_dir, label, crc32, index, size\n")
        out.write(card_meta_data)
        out.write("};\n\n")
        return identifier_index

    print(f"file genered: source/carde_data.c")

def generate_h_file(identifier_index):

    output_file_h = os.path.join(BASE_PATH, 'source', 'carde_data.h')

    # input_folder = os.path.join(BASE_PATH, 'e-cards', 'bin')
    # bin_files = [f for f in os.listdir(input_folder) if f.endswith('.bin')]

    with open(output_file_h, 'w') as f:
        f.write("#ifndef CARDE\n#define CARDE\n\n#include <stdint.h>\n\n")
        f.write("#ifdef __cplusplus\nextern \"C\" {\n#endif\n\n")

        f.write(f"#define CARD_LIST_SIZE {identifier_index}\n\n")
        struct_header = (
            "typedef struct {\n"
            "    const uint32_t* data;\n"
            "    const char* label;\n"
            "    uint32_t crc32;\n"
            "    int index;\n"
            "    int size;\n"
            "} CardEntry;"
        )
        
        f.write(struct_header)

        f.write(f"\n\nextern const CardEntry card_list[];")

        reversed_seed_candidate = challenge_4byte()
        f.write(f"\n\nconst uint32_t challenge_4bytes = 0x{reversed_seed_candidate:08X};\n")        
           
        f.write("#ifdef __cplusplus\n}\n#endif\n\n#endif // CARDE")

    print(f"file genered: source/carde_data.h")


def convert_raw_to_bin():

    input_folder = os.path.join(BASE_PATH, 'e-cards', 'raw')
    output_folder = os.path.join(BASE_PATH, 'e-cards', 'bin')
    exe_path = os.path.join(BASE_PATH, 'apps', 'nedcenc.exe')

    for file in os.listdir(input_folder):

        if file.lower().endswith('.raw'):
            input_file = os.path.join(input_folder, file)

            name_base = os.path.splitext(file)[0]
            dir_output = os.path.join(output_folder, f"{name_base}.bin")

            try:

                subprocess.run([exe_path, "-d", "-i", input_file, "-o", dir_output], check=True)
                print(f"[OK] decompres: {file} -> {name_base}.bin")

            except subprocess.CalledProcessError:

                print(f"[ERROR] Decompress error: {file}")

def main():
    try:
        convert_raw_to_bin()    
        generate_h_file(convert_bins_to_cpp())
        
    except Exception as e:
        print(f"As error ocurred: {e}")

main()