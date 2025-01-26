#!/usr/bin/env bash

# 1) Grab everything between the start of the index_ov2640_html_gz array and the closing "};"
head -n2 camera_index.h

sed -n '/const unsigned char index_ov2640_html_gz\[\] = {/,/^};/p' camera_index.h \
  >raw_array_block.txt

head -n2 raw_array_block.txt

sed -i '/const unsigned char index_ov2640_html_gz\[\] = {/d' raw_array_block.txt
head -n2 raw_array_block.txt

sed -i 's/};$//' raw_array_block.txt
head -n2 raw_array_block.txt

# 3) Remove all leading whitespace
sed -i 's/^[[:space:]]*//' raw_array_block.txt
head -n2 raw_array_block.txt

# 3) Strip out '0x', commas, spaces. Remove empty lines.
sed -i 's/0x//g; s/,//g; s/ //g; /^$/d' raw_array_block.txt
head -n2 raw_array_block.txt

# 4) Convert from hex text to a real binary file
xxd -r -p raw_array_block.txt index_ov2640.html.gz

# 5) Finally, decompress
gunzip -f index_ov2640.html.gz
echo "Decompression complete. Your HTML should be in 'index_ov2640.html'."
rm raw_array_block.txt
