#! /usr/bin/env python3

# TODO: Write new IG accounts to xlsx during runtime, not at the end of iterating through the error dict.

import logging
import sys
import csv
import os
import re
import webbrowser
import subprocess
import pandas
from collections import defaultdict
from typing import Dict, Union, List

import instaloader
from pypref import Preferences

# NOTE TO USER: use logging.DEBUG for testing, logging.CRITICAL for runtime
logging.basicConfig(stream=sys.stderr,
                    level=logging.DEBUG)

# Directories
ROOT_PICTURE_DIR = os.path.realpath("E:/OneDrive/Pictures")
pic_directories_dict: [str, str] = {
    os.path.join(ROOT_PICTURE_DIR, "NEED TO SORT (NSFW)"): os.path.join(ROOT_PICTURE_DIR, "NSFW"),
    os.path.join(ROOT_PICTURE_DIR, "NEED TO SORT (MSFW)"): os.path.join(ROOT_PICTURE_DIR, "MSFW"),
    os.path.join(ROOT_PICTURE_DIR, "NEED TO SORT (SFW)"): os.path.join(ROOT_PICTURE_DIR, "SFW")}
"""{ NEED_TO_SORT Directory : Output Directory }"""

# Data files
IG_DB_FILE = os.path.join(os.path.dirname(__file__), "ig_boys.xlsx")
"""The database file that contains pairs of Instagram accounts and the users's IRL names"""

boys_dict: Dict[str, str] = dict()
"""{ Account:  irl_name }"""

ig_db_file_fieldnames: List[str] = ["Account", "Name"]
"""These fieldnames are used by writerow() to add info to the Spreadsheet file"""

PHOTOGRAPHERS_DB_FILE: str = os.path.join(os.path.dirname(__file__),
                                          "photographers.txt")
"""A file that stores Instagram accounts associated with photographers"""
photographers_list: List[str] = list()
"""A list of known photographers compiled from PHOTOGRAPHERS_DB_FILE"""

# Error handling
error_dict: Dict[str, Dict[str, List[str]]] = dict()  # TODO: Convert this to a defaultdict?
"""{ Directory : { IG Name : LIST of full file paths } }"""
special_cases_types = ("Ipad Screenshot", "Edited Screenshot", "Twitter", "Other", "Unknown File Type")

# Counters
files_renamed_count: int = 0
new_files_successfully_processed: int = 0


def main():
    """
    This is the driver method for the entire app.
    :return:    None
    """
    # Gather globals
    global boys_dict
    global error_dict
    global files_renamed_count
    global photographers_list
    global IG_DB_FILE

    init_data()

    # Check ordering of existing sorted files.
    for subdir in pic_directories_dict.values():
        print_section("Checking ordering in " + str(os.path.join(ROOT_PICTURE_DIR, subdir)), "-")
        fix_numbering(os.path.join(ROOT_PICTURE_DIR, subdir))

    # Check all destination directories to see if all files are correctly named.
    for subdir in pic_directories_dict.values():
        find_unknown_boys_in_boys_dict(subdir)

    # Begin sorting new pics
    for subdir in pic_directories_dict:
        print_section("Sorting new pics in " + str(os.path.join(ROOT_PICTURE_DIR, subdir)), "-")
        sort_new_pictures(subdir, pic_directories_dict[subdir])

    # If any of the files failed to sort, begin error handling.
    if error_dict:
        logging.debug(print_section("ERROR DICTIONARY"))
        for dir in error_dict:
            logging.debug("\tDIRECTORY:" + dir)
            for ig_name in error_dict[dir]:
                logging.debug("\t\tInstagram Account: " + ig_name)
                # logging.debug("\t\t\tFILE LIST: " + str([os.path.basename(x) for x in error_dict[dir][ig_name]]))

        os.chdir(os.path.split(__file__)[0])  # Change to the directory of the script
        # TODO: Fix this broken section of code!!!
        for error_dir in list(error_dict):
            for ig_name in list(error_dict[error_dir]):
                # Double check to make sure that the boy has not been added to boys_dict during the operation of this script.
                if ig_name in boys_dict:
                    for filename in error_dict[error_dir][ig_name]:
                        name_file_to_next_available_name(filename, pic_directories_dict[error_dir], irl_name)
                    del error_dict[error_dir][ig_name]
                    if not error_dict[error_dir]:
                        del error_dict[error_dir]

                elif ig_name not in photographers_list and ig_name not in special_cases_types:
                    # Open the webpage for the problem file(s)
                    webbrowser.open("".join(["https://www.instagram.com/", ig_name]))

                    # Open the directory with the problem file(s)
                    os.startfile(os.path.dirname(error_dict[error_dir][ig_name][0]))
                    irl_name = input(
                        "\nWe have found a new account named \""
                        + ig_name + "\" that is not in our database. Enter the name of this boy "
                                    "OR type \"p\" if this is a photographer's account:").strip()
                    # Handle photographers
                    if irl_name == "p":
                        photographers_list.append(ig_name)
                        with open(PHOTOGRAPHERS_DB_FILE, 'a') as pf:
                            pf.write(ig_name + "\n")
                    # Handle new account names
                    else:
                        df = pandas.read_excel(IG_DB_FILE)
                        df = df.append(pandas.DataFrame([[ig_name, irl_name]], columns=df.columns))
                        df.to_excel(IG_DB_FILE, index=False)
                        logging.debug("NEW ACCOUNT LOGGED TO SPREADSHEET")
                        boys_dict[ig_name] = irl_name
                        for filename in error_dict[error_dir][ig_name]:
                            name_file_to_next_available_name(filename, pic_directories_dict[error_dir], irl_name)
                        del error_dict[error_dir][ig_name]
                        if not error_dict[error_dir]:
                            del error_dict[error_dir]

    # Handle all errors that have been found that are NOT photographers
    if error_dict:
        print_section("Problems that are NOT related to photographers", "-")
        for error_dir in list(error_dict):
            for ig_name in list(error_dict[error_dir]):
                # Try to handle special file cases
                if ig_name in special_cases_types:
                    handle_special_account(error_dir, ig_name, False)

    # Handle any remaining errors that involve photographer accounts.
    if error_dict:
        print_section("Problems with photographer files", "-")
        for error_dir in list(error_dict):
            for ig_name in list(error_dict[error_dir]):
                if ig_name in photographers_list:
                    handle_special_account(error_dir, ig_name, True)

    # LAST CHANCE, handle each remaining error file individually.
    if error_dict:
        print_section("Handling error files individually", "-")
        for error_dir in list(error_dict):
            for ig_name in list(error_dict[error_dir]):
                for file in list(error_dict[error_dir][ig_name]):
                    handle_individual_file(error_dir, ig_name, file)

    # If we cannot figure out ANYTHING about this boy/file, just print the file name.
    if error_dict:
        print_section("ERRORS THAT COULD NOT BE RESOLVED", "*")
        for error_dir in list(error_dict):
            for ig_name in list(error_dict[error_dir]):
                for filename in error_dict[error_dir][ig_name]:
                    logging.debug(filename)

    # In the normal runtime we should solve all our problems.
    # If this is not the case then we should debug by printing the error_dict
    # if error_dict:
    #    print_error_dict()

    # Print a final report
    print_section("FINAL REPORT", "*")
    logging.info("NUMBER OF FILES RENAMED: " + str(files_renamed_count))
    logging.info("NUMBER OF NEW FILES SORTED: " + str(new_files_successfully_processed))
    logging.info("\nTOTAL ERROR ACCOUNTS REMAINING: " + str(sum(len(v) for v in error_dict.values())) + "\n")


def sort_new_pictures(in_dir, out_dir):
    """
    This method sorts new pictures from an input directory to an output directory.

    :param in_dir:  The original location of the pictures.
    :param out_dir: The output directory that the pictures will be moved to once they have been processed.
    :return:        None
    """
    # Globals
    global error_dict
    global boys_dict

    # adjust current working directory (cwd)
    os.chdir(in_dir)

    # Find the file paths for all the files in the input folder and add them to a list.
    infile_list: List[Union[str, bytes, os.PathLike]] = list()
    for file in os.listdir(in_dir):
        if file.lower() == "desktop.ini" or file.lower() == "thumbs.db":
            pass
        else:
            infile_list.append(os.path.realpath(os.path.join(in_dir, file)))

    # Initialize other variables
    current_boy_ig_or_irl_name = ""
    current_file_basename = ""
    for full_file_path in list(infile_list):
        current_file_basename = os.path.basename(full_file_path)

        # Get the IG Name for the file.
        # TODO: Shift all this to get_IG_name_from_filename()
        # TODO: This applies to all 3 cases. Convert the first 2
        # Case 1: The file is named after the boy's first and last name.
        # EXAMPLE: "John Smith.jpg."
        if current_file_basename.split('.')[0] in boys_dict.values():
            current_boy_ig_or_irl_name = current_file_basename.split('.')[0]
        # Case 2: The file is named after the boy's first and last name BUT is more complicated.
        # EXAMPLE: "John Smith (2).jpg."
        elif re.search(r".+?(?![^().0-9])", str(current_file_basename.split('.')[0])) is not None and re.search(
                r".+?(?![^().0-9])", str(current_file_basename.split('.')[0])).group(0).strip() in boys_dict.values():
            current_boy_ig_or_irl_name = re.search(
                r".+?(?![^().0-9])", str(current_file_basename.split('.')[0])).group(0).strip()
        # Case 3: The file is not simply the boy's first and last name.
        # Use a sorting method to figure out what to do with this file.
        else:
            current_boy_ig_or_irl_name = get_IG_name_from_filename(full_file_path)

        # Attempt to rename and move the file based on its IG Name.
        if current_boy_ig_or_irl_name in boys_dict:
            name_file_to_next_available_name(full_file_path, out_dir, boys_dict[current_boy_ig_or_irl_name])
        elif current_boy_ig_or_irl_name in boys_dict.values():
            # TODO: Do I need this?
            name_file_to_next_available_name(full_file_path, out_dir, current_boy_ig_or_irl_name)
        else:
            # A problem exists with this file. Initiate error handling.
            logging.info("-Could not process: " + str(current_file_basename))

            if in_dir not in error_dict:
                error_dict[in_dir] = defaultdict(list)
            error_dict[in_dir][current_boy_ig_or_irl_name].append(full_file_path)

    if len(infile_list) > 0:
        logging.info(infile_list)
    logging.info("Done sorting from " + str(in_dir) + " to " + str(out_dir) + ".")


def get_IG_name_from_filename(full_file_path):
    """
    This is a helper method to attempt to rename a file based off of the name that it originally had when it was downloaded.

    :param full_file_path: The path to the file that we want the IG name from.
    :return: The IG name of the boy associated with this file.
    """
    basename = os.path.basename(str(full_file_path))
    # Case 1: Instagram batch downloader
    # EXAMPLE: jefe004_2020-07-24_18-16-11_1
    if re.search(r"^.+?(?=_\d\d\d\d-\d\d)", basename) is not None:
        return re.search(r"^.+?(?=_\d\d\d\d-\d\d)", basename).group(0)
    # Case 2: Screenshots
    # EXAMPLE: IMG_0005.png (iPad screenshot)
    elif "Ipad Screenshot" in basename or re.search(r"^IMG_[0-9]{4}\.", basename) is not None:
        return "Ipad Screenshot"
    # Case 3: FastSave Android App
    elif "___" in basename:
        return re.search(r".+?(?=_[0-9]*_{3})(?!_{4,})|.+?(?=_{3,})(?!_{4,})",
                         basename).group(0)
    # Case 4: Chrome Downloader for Instagram
    # EXAMPLE: boyname_12345678_123456789012345...
    elif re.search(r"_[0-9]{8}_[0-9]{15}", basename) is not None:
        return re.search(r".+?(?=_[0-9]{8}_[0-9]{15})", basename).group(0)
    # Case 5 Chrome Downloader for Instagram (alternate)
    # EXAMEPL: boyname_10576075_402670663234919_903188201_n.jpg
    elif re.search(r"_[0-9]{6,}_[0-9]{15,}_[0-9]{8,}_n", basename) is not None:
        return re.search(r".+?(?=_[0-9]{6,}_[0-9]{15,}_[0-9]{8,}_n)", basename).group(0)
    # Case 6: Edited Screenshot from Android
    # EXAMPLE: 123456(78)_123456...
    elif re.search(r"^[0-9]{6,8}_[0-9]{6}", basename) is not None:
        return "Edited Screenshot"
    # Case 7: Twitter
    # EXAMPLE: IMG_12345678_123456...
    elif re.search(r"^IMG_[0-9]{8}_[0-9]{6}", basename) is not None:
        return "Twitter"
    # Case 8: other screenshot??
    # EXAMPLE: 1234-12-12...
    elif re.search(r"^[0-9]{4}-[0-9]{2}-[0-9]{2}", basename) is not None:
        # TODO: Handle this more eloquently
        logging.debug("OTHER FILE TYPE FOUND. THIS MAY CAUSE ERRORS!!!!!!!!!!")
        return "Other"
    else:
        logging.debug("UNKNOWN FILE TYPE FOUND. THIS IS LIKELY TO CAUSE ERRORS!!!!!!!!!!")
        return "Unknown File Type"


def fix_numbering(dir_to_renumber):
    """
    This renumbers ALL the files in a directory (John Smith 1.png, John Smith 2.png, etc.).

    :param dir_to_renumber: A directory of files that need to be renumbered
    :return: None
    """
    # Initialize variables
    previous_boy_name = None
    current_boy_name = ""
    current_pic_counter = -1  # How many pictures of this boy?
    max_pic_counter = 1
    problems_exist = False

    current_boy_pic_list = [None]
    file_name_as_list_of_name0_and_ext1 = list()

    # Get all the shit in my current working directory
    os.chdir(dir_to_renumber)  # changes current working directory
    files_list = os.listdir(os.getcwd())
    if os.path.isfile(os.path.join(dir_to_renumber, "Thumbs.db")):
        files_list.remove("Thumbs.db")
    if os.path.isfile(os.path.join(dir_to_renumber, "desktop.ini")):
        files_list.remove("desktop.ini")

    # Compile files_list into boy_names_and_numbers_list_of_lists, we will sort it in a minute
    for current_file in files_list:
        file_name_as_list_of_name0_and_ext1 = list(os.path.splitext(os.path.basename(current_file)))

        current_boy_name = re.search(r"[^0-9]+", file_name_as_list_of_name0_and_ext1[0]).group().rstrip()
        current_pic_counter = int(re.search(r"[0-9]+", file_name_as_list_of_name0_and_ext1[0]).group())  # pic_counter

        if previous_boy_name == current_boy_name:
            if current_pic_counter > max_pic_counter:
                max_pic_counter = current_pic_counter
            if max_pic_counter > len(current_boy_pic_list) + 1:
                current_boy_pic_list[len(current_boy_pic_list): max_pic_counter + 1] = [None] * (
                        max_pic_counter - len(current_boy_pic_list))
            current_boy_pic_list.insert(current_pic_counter, current_file)
            if current_pic_counter != max_pic_counter and current_boy_pic_list[current_pic_counter + 1] is None:
                del current_boy_pic_list[current_pic_counter + 1]

        else:
            counter_should_be = 1
            loop_counter = 0
            for picture_of_current_boy in current_boy_pic_list[0:max_pic_counter + 1]:
                if picture_of_current_boy:
                    # Initialize variables
                    pic_file_name_as_list_of_name0_and_ext1 = list(
                        os.path.splitext(os.path.basename(picture_of_current_boy)))
                    inner_current_boy_name = re.search(r"[^0-9]+",
                                                       pic_file_name_as_list_of_name0_and_ext1[0]).group().rstrip()
                    inner_current_pic_counter = int(
                        re.search(r"[0-9]+", pic_file_name_as_list_of_name0_and_ext1[0]).group())  # pic_counter

                    if inner_current_pic_counter != counter_should_be:
                        problems_exist = True
                        logging.info(">>>NUMBERING PROBLEM WITH FILE: " + str(picture_of_current_boy))
                        new_file_name_and_ext = inner_current_boy_name + " " + str(counter_should_be) + str(
                            pic_file_name_as_list_of_name0_and_ext1[1])
                        os.rename(os.path.realpath(os.path.join(dir_to_renumber, picture_of_current_boy)),
                                  os.path.realpath(os.path.join(dir_to_renumber, new_file_name_and_ext)))
                        logging.info(
                            "\tFIXED: " + str(picture_of_current_boy) + " renamed to: " + new_file_name_and_ext)
                        global files_renamed_count
                        files_renamed_count += 1

                    counter_should_be += 1
                loop_counter += 1

            max_pic_counter = current_pic_counter
            current_boy_pic_list = [None]  # reset list
            if max_pic_counter > len(current_boy_pic_list) + 1:
                current_boy_pic_list[len(current_boy_pic_list): max_pic_counter + 1] = [None] * (
                        max_pic_counter - len(current_boy_pic_list))
            current_boy_pic_list.insert(current_pic_counter, current_file)
        previous_boy_name = current_boy_name

    if problems_exist:
        logging.info("Done fixing numbering in " + str(dir_to_renumber) + ".")
    else:
        logging.info("No problems exist in " + str(dir_to_renumber) + ".\n")


def handle_special_account(error_dir, error_ig_name, is_photographer):
    """
    This is an error handling function.
    If the normal sorting fail then we attempt to resolve the problem via user input.

    :param error_dir:  The directory of the error file(s).
    :param error_ig_name:   The name of an IG account. There may be MULTIPLE errors associated with a single account.
                            If that is the case then they will be handled as a batch.
    :param is_photographer: A Boolean value that determines if this is a photographer's account.
                            If it is, we may have to add a new ig_name to our list to sort the file.

    :return:    None
    """

    global error_dict

    num_special_files = len(error_dict[error_dir][error_ig_name])
    if is_photographer:
        logging.info("\nWe have found " + str(num_special_files)
                     + " pictures from Instagram photographer \"" + error_ig_name
                     + "\" in this batch, including:")
    else:
        logging.info("\nWe have found " + str(num_special_files)
                     + " pictures from Instagram account \"" + error_ig_name
                     + "\" in this batch, including:")

    for filename in error_dict[error_dir][error_ig_name]:
        logging.info(filename)

    os.chdir(os.path.dirname(os.path.realpath(__file__)))

    if num_special_files > 1:
        # Open the file's location in file explorer
        # command = "explorer /select, \"" + error_dict[error_dir][error_ig_name][0] + "\""
        command = "explorer /select, \"" + error_dict[error_dir][error_ig_name][0] + "\""
        subprocess.Popen(command)
        pics_are_same_boy = input("\nAre all these pictures of the same boy? (y/n)").strip().lower()
    else:
        pics_are_same_boy = "yes"
        # Open the file itself
        os.startfile(error_dict[error_dir][error_ig_name][0])

    if pics_are_same_boy == "y" or pics_are_same_boy == "yes":
        # Open the Instagram's page in web browser
        if error_ig_name not in special_cases_types:
            webbrowser.open("".join(["https://www.instagram.com/", error_ig_name]))

        boy_irl_name = input("Please enter the boy's name: ").strip()
        if boy_irl_name in boys_dict or boy_irl_name in boys_dict.values():
            logging.info("I found him in the database!\n")
            for filename in error_dict[error_dir][error_ig_name]:
                name_file_to_next_available_name(filename, pic_directories_dict[os.path.dirname(filename)],
                                                 boy_irl_name)
            del error_dict[error_dir][error_ig_name]
            if not error_dict[error_dir]:
                del error_dict[error_dir]
        else:
            user_input = input("That didn't work. Do you want to track a new IG account? (y/n)").strip()
            if user_input.lower() == "y" or user_input.lower() == "yes":
                boy_ig_name = input("Enter the name of this boy's IG account:").strip()

                # Write change to ig_boys.csv
                os.chdir(os.path.split(__file__)[0])
                df = pandas.read_excel(IG_DB_FILE)
                df = df.append(pandas.DataFrame([[boy_ig_name, boy_irl_name]], columns=df.columns))
                df.to_excel(IG_DB_FILE, index=False)

                # Update boys_dict
                boys_dict[boy_ig_name] = boy_irl_name

                # Loop over the problem files associated with this new account and fix them.
                for file in error_dict[error_dir][error_ig_name]:
                    name_file_to_next_available_name(file, pic_directories_dict[error_dir], boy_irl_name)
                del error_dict[error_dir][error_ig_name]
                if not error_dict[error_dir]:
                    del error_dict[error_dir]
            else:
                logging.info("Ok, we'll skip this account for now.")
    else:
        logging.info("Ok, we'll handle these files individually later.")


def handle_individual_file(error_dir, error_ig_name, full_file_path):
    """
    This function exists if EVERYTHING else has failed and we are giving a file
    one last shot to be identified before we call it quits.
    This usually means that there are multiple pictures from the same source but they are of different boys
    so they must be sorted individually.

    :param error_dir:  The directory of the error file.
    :param error_ig_name:   The name of an IG account.
    :param full_file_path:  The path for the file we are investigating.
    :return:
    """
    global error_dict

    # Attempt to open the IG page for the account associated with the picture.
    if error_ig_name not in special_cases_types:
        webbrowser.open("".join(["https://www.instagram.com/", error_ig_name]))

    # Open the picture
    os.startfile(full_file_path)

    logging.info("\nAnalyzing file: " + full_file_path)
    boy_irl_name = input("Last chance. Please enter this boy's name: ").strip()

    if boy_irl_name in boys_dict.values():
        logging.info("I found " + boy_irl_name + " in the database!")
        name_file_to_next_available_name(full_file_path, pic_directories_dict[os.path.dirname(full_file_path)],
                                         boy_irl_name)
        error_dict[error_dir][error_ig_name].remove(full_file_path)
        if len(error_dict[error_dir][error_ig_name]) == 0:
            del error_dict[error_dir][error_ig_name]
            if not error_dict[error_dir]:
                del error_dict[error_dir]
    else:
        user_input = input("That didn't work. Do you want to track a new IG account? (y/n)").strip()
        if user_input.lower() == "y" or user_input.lower() == "yes":
            boy_ig_name = input("Enter this boy's account name: ").strip()
            user_input = input(
                "You said this boy's name was " + boy_irl_name + ". Is that correct? (y/n)").strip().lower()
            if user_input != "y" and user_input != "yes":
                boy_irl_name = input("Please enter this boy's name: ").strip()
            os.chdir(os.path.split(__file__)[0])  # Change to the directory of the script
            df = pandas.read_excel(IG_DB_FILE)
            df = df.append(pandas.DataFrame([[boy_ig_name, boy_irl_name]], columns=df.columns))
            df.to_excel(IG_DB_FILE, index=False)
            boys_dict[boy_ig_name] = boy_irl_name
            name_file_to_next_available_name(full_file_path, pic_directories_dict[error_dir], boy_irl_name)
            error_dict[error_dir][error_ig_name].remove(full_file_path)
            if len(error_dict[error_dir][error_ig_name]) == 0:
                del error_dict[error_dir][error_ig_name]
                if not error_dict[error_dir]:
                    del error_dict[error_dir]
        else:
            logging.debug("Unable to process this file. Please attempt manual fixes.")


def name_file_to_next_available_name(full_filename, out_dir, boy_irl_name):
    """
    This function takes a file and sorts it to an output directory.

    :param full_filename:    The file to sort.
    :param out_dir:         The destination directory that the file will be sorted into.
    :param boy_irl_name:    The boy's IRL name.
    :return:        None.
    """

    if type(boy_irl_name) is not str:
        raise TypeError("boy_irl_name is supposed to be a string but is instead a " + str(type(boy_irl_name)) + ".")

    os.chdir(os.path.dirname(full_filename))

    # Now dump all the files in the output directory into a list.
    os.chdir(out_dir)
    files_in_output_dir = os.listdir(out_dir)
    file_names_without_ext_in_output_directory = [file.split(".")[0] for file in
                                                  files_in_output_dir]  # List comprehension

    # Find the right number for our new filename.
    next_number_for_filename = 1
    new_filename_without_ext = ""
    while True:
        new_filename_without_ext = boy_irl_name + " " + str(next_number_for_filename)
        if new_filename_without_ext in file_names_without_ext_in_output_directory:
            next_number_for_filename += 1
        else:
            break
    new_filename_with_ext = new_filename_without_ext + os.path.splitext(full_filename)[1]

    os.rename(full_filename, os.path.join(out_dir, new_filename_with_ext))
    logging.debug("+" + str(full_filename) + " successfully sorted to " + out_dir + " as " + new_filename_with_ext)
    global new_files_successfully_processed
    new_files_successfully_processed += 1


def init_data():
    """
    This is how we fire up our dictionary and list files that contain all the info on existing files.

    :return:    None
    """
    # Import globals
    global boys_dict
    global IG_DB_FILE
    global PHOTOGRAPHERS_DB_FILE
    global photographers_list

    # Read Excel file into a dictionary
    os.chdir(os.path.dirname(os.path.realpath(__file__)))
    df = pandas.read_excel("ig_boys.xlsx")
    for idx, row in df.iterrows():
        if pandas.isnull(row["Name"]):
            if pandas.isnull(row["Account"]):
                raise Exception("There is an empty row in " + str(
                    IG_DB_FILE) + " at line " + str(idx) + ". Please fix this.")
            else:
                webbrowser.open("".join(["https://www.instagram.com/", row["Account"]]))
                raise Exception("Account \"" + str(row["Account"]) + "\" has caused an error in " + str(
                    IG_DB_FILE) + ". All account names MUST have a corresponding IRL name but this one is blank.")
        else:
            if pandas.isnull(row["Account"]):
                # If we only have the boy's name then he is { John Smith : John Smith }
                boys_dict[row["Name"]] = row["Name"]
            else:
                # If we have all the info (best case scenario)
                boys_dict[row["Account"]] = row["Name"]

    with open(PHOTOGRAPHERS_DB_FILE, 'r') as f:
        photographers_list = f.read().splitlines()


def print_section(section_title, symbol="*"):
    """
    A helper method to print our output to the console in a pretty fashion

    :param section_title:   The name of the section we are printing.
    :param symbol:      The symbol to repetetively print to box in our output. This will usually be a '*'.
    :return:    None
    """
    logging.info("\n" + (symbol * 50) + "\n" + section_title + "\n" + (symbol * 50) + "\n")


def print_error_dict():
    """
    A helper method that prints out error_dict.
    This is NOT used in the ordinary running of the program unless the script is modified to do so.

    :return: None
    """
    global error_dict
    for error_dir in error_dict:
        logging.debug("KEY (error_dir): " + str(error_dir))
        for ig_name in error_dict[error_dir]:
            logging.debug("\tIG NAME: " + ig_name)
            for fname in error_dict[error_dir][ig_name]:
                logging.debug("\t\tFILE: " + fname)
    if not error_dict:
        logging.debug("error_dict is empty!")
    input("\nPress Enter to continue the script...")


def find_unknown_boys_in_boys_dict(dir):
    """
    Finds all the files in a directory that are not in boys_dict

    :param dir: Directory to search in
    :return: None
    """
    # Get a list of all the files in this directory
    full_file_paths = []
    for file in os.listdir(dir):
        full_file_paths.append(os.path.realpath(os.path.join(dir, file)))

    error_boys_list = []
    for full_file_path in full_file_paths:
        current_file_basename = os.path.basename(full_file_path)
        if current_file_basename.lower() == "desktop.ini" or current_file_basename.lower() == "thumbs.db":
            continue

        boy_name = re.search(r".+?(?=[" "][0-9]+)", current_file_basename).group(0).strip()
        if boy_name not in boys_dict.values() and boy_name not in error_boys_list:
            error_boys_list.append(boy_name)

    if error_boys_list:
        print_section("UNKNOWN BOYS EXIST IN " + str(dir), "*")

    for boy_name in error_boys_list:
        logging.info(boy_name)


if __name__ == '__main__':
    main()
