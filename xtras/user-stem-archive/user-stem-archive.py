#
# OUROVEON DATA EXTRACTION SAMPLE
#
# takes a populated Warehouse database and - ideally - a fully cached set of stems (we do no downloading here)
# and extracts all the stems for a given user, re-organising them while copying out local to this script
#
# this is a very simple test written by someone who doesn't program in python. do your worst
#
#
# set [ouro_cache_path] and [ouro_username] to wherever your cache lives and whom you are filtering on

import sqlite3
import os
import shutil

from pathlib import Path

# where the cache lives
ouro_cache_path = Path("E:/") / "Audio" / "Ouroveon" / "cache" / "common"

# who to go digging for
ouro_username = "bingly_bungus"


# file extensions to append for the given MIME type in the db
mime_to_extension = {
    "audio/ogg":    ".ogg",
    "audio/flac":   ".flac"
}

# given a path, ensure we can begin copying things in by creating each directory given in turn
def create_directory_if_not_exists(path):
    try:
        os.makedirs(path, exist_ok=True)
    except OSError as e:
        if e.errno != errno.EEXIST:
            raise

# roundtrip the (technically) floating point value in BPM through an integer conversion, giving us an easy to read result
def round_bpm(string_value):
  try:
    integer_value = int(string_value)
    return str(integer_value)
  except ValueError:
    raise

# given the preset name from the DB, return it or a fallback value if it's blank
def filter_preset_name(input_string, fallback_value="Unknown"):
  return input_string or fallback_value


# credit to Google Gemini
def sanitize_filename(filename):
    """Sanitizes a filename to ensure it's safe for use on most operating systems.

    Args:
        filename: The original string to be sanitized.

    Returns:
        A sanitized string suitable for use as a filename.
    """

    valid_chars = "-_.() {}[]abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"
    sanitized = ''.join(c for c in filename if c in valid_chars)

    # Optional: Replace spaces with underscores for better compatibility
    sanitized = sanitized.replace(" ", "_")

    return sanitized


# open a connection to the warehouse database
db_path = ouro_cache_path / "warehouse.db3"
try:
    conn = sqlite3.connect(db_path)
    print("Connection to OUROVEON Warehouse database : ok")
except sqlite3.Error as e:
    print("ERROR | cannot connect to database:", e)
    print("ERROR | path tried:", db_path)
    quit(1)


# pull the user stem data back out
cursor = conn.cursor()
cursor.execute(f"select OwnerJamCID, StemCID, BPMrnd, CreationTime, PresetName, FileMIME, Instrument from stems where CreatorUserName is '{ouro_username}' order by CreationTime desc limit 2000")
rows = cursor.fetchall()


# copy each in turn to an organised folder local to this script
for row in rows:
    stem_path_in_cache = ouro_cache_path / "stem_v2" / row[0] / row[1][0] / row[1]
    if os.path.exists(stem_path_in_cache):
        
        stem_copy_destination_path = Path(ouro_username) / round_bpm(row[2]) 
        create_directory_if_not_exists( stem_copy_destination_path )
        stem_file_destination = stem_copy_destination_path / sanitize_filename( f"{row[3]}_{row[1][0:4]}_i{row[6]}_{filter_preset_name(row[4])}{mime_to_extension[row[5]]}" )

        print(stem_file_destination)
        shutil.copy(stem_path_in_cache, stem_file_destination)

    else:
        print("NOT FOUND |", stem_path)

conn.close()