import csv
import os
import re
import webbrowser
import subprocess

# TODO: Eliminate the need for "NEED TO SORT" folders.

# Directories
root_picture_directory = os.path.realpath("E:/OneDrive/Pictures")
pic_directories_dict = {
    os.path.join(root_picture_directory, "NEED TO SORT (NSFW)"): os.path.join(root_picture_directory, "NSFW"),
    os.path.join(root_picture_directory, "NEED TO SORT (MSFW)"): os.path.join(root_picture_directory, "MSFW"),
    os.path.join(root_picture_directory, "NEED TO SORT (SFW)"): os.path.join(root_picture_directory, "SFW")}

# Data files
boys_dictionary_file = "boys.csv"  # You can call this whatever you want.
boys_dict = dict()  # { "Account":  "irl_name" }
boys_dict_file_field_names = ["Account", "Name"]  # used by writerow(), MUST be a list
photographers_list_file = "photographers.txt"
photographers_list = dict()

# Error handling
error_boys_dict = dict()  # { Directory : { IG Name : LIST of full file paths } }
special_cases_types = ["Screenshot", "Twitter", "Other"]

# Counters
files_renamed_count = 0
new_files_successfully_processed = 0


def main():
    """
    This is the driver method for the entire app.

    :return:    None
    """
    # Globals
    global boys_dict
    global error_boys_dict
    global root_picture_directory
    global files_renamed_count
    global pic_directories_dict
    global new_files_successfully_processed
    global photographers_list
    global special_cases_types

    initialize_data()

    # Check ordering of existing sorted files.
    for subdir in pic_directories_dict.values():
        print_section("Checking ordering in " + str(os.path.join(root_picture_directory, subdir)), "-")
        fix_numbering(os.path.join(root_picture_directory, subdir))

    # Prepping done, now sort new pics
    for subdir in pic_directories_dict:
        print_section("Sorting new pics in " + str(os.path.join(root_picture_directory, subdir)), "-")
        sort_new_pictures(subdir, pic_directories_dict[subdir])

    # If any of the files failed to sort, begin error handling.
    if error_boys_dict:
        os.chdir(os.path.split(__file__)[0])
        with open(boys_dictionary_file, 'a', newline="") as f:
            global boys_dict_file_field_names
            writer = csv.DictWriter(f, fieldnames=boys_dict_file_field_names)
            for error_dir in list(error_boys_dict):
                for ig_name in error_boys_dict[error_dir]:
                    if ig_name not in photographers_list and ig_name not in special_cases_types:
                        webbrowser.open("".join(["https://www.instagram.com/", ig_name]))
                        irl_name = input(
                            "We have found a new account named \"" + ig_name + "\" that is not in our database. What is the name of this boy?")
                        writer.writerow({"Account": ig_name,
                                         "Name": irl_name})
                        boys_dict[ig_name] = irl_name
                        for filename in error_boys_dict[error_dir][ig_name]:
                            name_file_to_next_available_name(filename, pic_directories_dict[error_dir], irl_name)
                        error_boys_dict[error_dir].pop(ig_name)

    # Handle all errors that have been found that are NOT photographers
    if error_boys_dict:
        print_section("Problems that are NOT related to photographers", "-")
        for error_dir in list(error_boys_dict):
            for ig_name in list(error_boys_dict[error_dir]):
                # Try to handle special file cases
                if ig_name not in special_cases_types:
                    handle_special_account(error_dir, ig_name)

    # NOW handle any remaining errors that involve photographers.
    if error_boys_dict:
        print_section("Problems with photographer files", "-")
        for error_dir in list(error_boys_dict):
            for ig_name in list(error_boys_dict[error_dir]):
                if ig_name in photographers_list:
                    handle_special_account(error_dir, ig_name)

    # If we cannot figure out ANYTHING about this boy/file, just print the file name.
    if error_boys_dict:
        print_section("ERRORS THAT COULD NOT BE RESOLVED", "*")
        for error_dir in list(error_boys_dict):
            for ig_name in error_boys_dict[error_dir]:
                for filename in error_boys_dict[ig_name]:
                    print(filename)

    # Print a final report
    print_section("FINAL REPORT", "*")
    print("NUMBER OF FILES RENAMED: " + str(files_renamed_count))
    print("NUMBER OF NEW FILES SORTED: " + str(new_files_successfully_processed))
    print("\nTOTAL ERRORS: " + str(sum(len(v) for v in error_boys_dict.values())) + "\n")

    # Open the directories with problem files
    error_directories = list()
    for subdict in list(error_boys_dict):
        for list_of_paths in subdict.values():
            for path in list_of_paths:
                if not os.path.dirname(path) in error_directories:
                    error_directories.append(os.path.dirname(path))
                    proc = subprocess.Popen(os.path.dirname(path),
                                            shell=True)  # We open the CSV file for the user is when all else has failed.

    # Open the Instagram pages for problem accounts
    for subdict in list(error_boys_dict):
        for ig_name in subdict:
            if ig_name in photographers_list or ig_name == "Screenshot" or ig_name == "Twitter" or ig_name == "Other":
                continue
            print(ig_name)
            webbrowser.open("".join(["https://www.instagram.com/", ig_name]))


def sort_new_pictures(in_dir, out_dir):
    """
    This method sorts new pictures from an input directory to an output directory.

    :param in_dir:  The original location of the pictures.
    :param out_dir: The output directory that the pictures will be moved to once they have been processed.
    :return:        None
    """
    # Globals
    global error_boys_dict
    global boys_dict

    # adjust current working directory (cwd)
    os.chdir(in_dir)

    # Find the file paths for all the files in the input folder and add them to a list.
    file_paths = []
    for folder, subs, files in os.walk(in_dir):
        for filename in files:
            file_paths.append(os.path.abspath(os.path.join(folder, filename)))

    # Initialize other variables
    current_boy_ig_name = ""
    current_file_basename = ""
    for full_file_path in file_paths:
        current_file_basename = os.path.basename(full_file_path)
        if current_file_basename.lower() == "desktop.ini" or current_file_basename.lower() == "thumbs.db":
            continue

        # Get the IG Name for the file.
        if current_file_basename.split('.')[0] in boys_dict.values():
            current_boy_ig_name = current_file_basename.split('.')[0]
        else:
            current_boy_ig_name = get_IG_name_from_filename(full_file_path)

        # Attempt to rename and move the file based on its IG Name.
        if current_boy_ig_name in boys_dict:
            name_file_to_next_available_name(full_file_path, out_dir, boys_dict[current_boy_ig_name])
        else:
            # A problem exists with this file. Initiate error handling.
            print(">>>Could not process: " + str(current_file_basename))

            if in_dir not in error_boys_dict:
                error_boys_dict[in_dir] = dict()

            # Add the current error to the error_boys_dict
            if current_boy_ig_name in error_boys_dict[in_dir]:
                error_boys_dict[in_dir][current_boy_ig_name].append(full_file_path)
            else:
                error_boys_dict[in_dir][current_boy_ig_name] = [full_file_path]

    if len(file_paths) > 0:
        print()
    print("Done sorting from " + str(in_dir) + " to " + str(out_dir) + ".")


def get_IG_name_from_filename(full_file_path):
    """
    This is a helper method to attempt to rename a file based off of the name that it originally had when it was downloaded.

    :param full_file_path: The path to the file that we want the IG name from.
    :return: None
    """
    basename = os.path.basename(str(full_file_path))
    # Case 1: Screenshots
    if "Screenshot" in basename:
        return "Screenshot"
    # Case 2: FastSave Android App
    elif "___" in basename:
        print("#2!!!!!!!!")
        return re.search(r".+?(?=_[0-9]*_{3})(?!_{4,})|.+?(?=_{3,})(?!_{4,})",
                         basename).group(0)
    # Case 3: Chrome Downloader for Instagram
    # EXAMPLE: _12345678_123456789012345...
    elif re.search(r"_[0-9]{8}_[0-9]{15}", basename) is not None:
        return re.search(r".+?(?=_[0-9]{8}_[0-9]{15})", basename).group(0)
    # Case 4: Edited Screenshot from phone
    # EXAMPLE: 123456(78)_123456...
    elif re.search(r"^[0-9]{6,8}_[0-9]{6}", basename) is not None:
        return "Edited Screenshot"
    # Case 5: Twitter
    # EXAMPLE: IMG_12345678_123456...
    elif re.search(r"^IMG_[0-9]{8}_[0-9]{6}", basename) is not None:
        return "Twitter"
    # Case 6: other screenshot??
    # EXAMPLE: 1234-12-12...
    elif re.search(r"^[0-9]{4}-[0-9]{2}-[0-9]{2}", basename) is not None:
        return "Other"
    else:
        proc = subprocess.Popen(os.path.dirname(os.path.abspath(full_file_path)), shell=True)
        raise Exception(
            str(full_file_path) + " in directory " + str(os.path.dirname(
                os.path.abspath(full_file_path))) + " is an unknown file type!")


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

    current_boy_list = [None]
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
            if max_pic_counter > len(current_boy_list) + 1:
                current_boy_list[len(current_boy_list): max_pic_counter + 1] = [None] * (
                        max_pic_counter - len(current_boy_list))
            current_boy_list.insert(current_pic_counter, current_file)
            if current_pic_counter != max_pic_counter and current_boy_list[current_pic_counter + 1] is None:
                current_boy_list.pop(current_pic_counter + 1)

        else:
            counter_should_be = 1
            loop_counter = 0
            for picture_of_current_boy in current_boy_list[0:max_pic_counter + 1]:
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
                        print(">>>NUMBERING PROBLEM WITH FILE: " + str(picture_of_current_boy))
                        new_file_name_and_ext = inner_current_boy_name + " " + str(counter_should_be) + str(
                            pic_file_name_as_list_of_name0_and_ext1[1])
                        os.rename(dir_to_renumber + "\\" + picture_of_current_boy,
                                  dir_to_renumber + "\\" + new_file_name_and_ext)
                        print("FIXED: " + str(picture_of_current_boy) + " renamed to: " + new_file_name_and_ext)
                        global files_renamed_count
                        files_renamed_count += 1

                    counter_should_be += 1
                loop_counter += 1

            max_pic_counter = current_pic_counter
            current_boy_list = [None]  # reset list
            if max_pic_counter > len(current_boy_list) + 1:
                current_boy_list[len(current_boy_list): max_pic_counter + 1] = [None] * (
                        max_pic_counter - len(current_boy_list))
            current_boy_list.insert(current_pic_counter, current_file)
        previous_boy_name = current_boy_name

    if problems_exist:
        print()  # Add a whitespace to separate the issues from the "Done" statement
    print("Done fixing numbering in " + str(dir_to_renumber) + ".")


def handle_special_account(error_file_dir, error_ig_name):
    """
    This is an error handling function. If the normal sorting fail then we attempt to resolve the problem via user input.

    :param error_file_dir:  The directory of the error file(s).
    :param error_ig_name:   The name of an IG account. There may be MULTIPLE errors associated with a single account.
                            If that is the case then they will be handled as a batch.
    :return:    None
    """
    global root_picture_directory
    global error_boys_dict
    global boys_dictionary_file

    num_special_files = len(error_boys_dict[error_file_dir][error_ig_name])
    print("\nWe have found " + str(num_special_files)
          + " pictures from Instagram account \"" + error_ig_name
          + "\" in this batch, including:")
    for filename in error_boys_dict[error_file_dir][error_ig_name]:
        print(filename)

    os.chdir(os.path.dirname(os.path.realpath(__file__)))
    os.startfile(error_file_dir)

    webbrowser.open(
        "".join(
            ["https://www.instagram.com/", get_IG_name_from_filename(error_boys_dict[error_file_dir][error_ig_name])]))
    if num_special_files > 1:
        pics_are_same_boy = input("\nAre all these pictures of the same boy? (y/n)").lower()
    else:
        pics_are_same_boy = "yes"
    if pics_are_same_boy == "y" or pics_are_same_boy == "yes":
        boy_irl_name = input("Please enter the boy's name: ")  # TODO: Add an "add photographer" option here.
        if boy_irl_name in boys_dict or boy_irl_name in boys_dict.values():
            print("I found him!\n")
            for filename in error_boys_dict[error_file_dir][error_ig_name]:
                name_file_to_next_available_name(filename, pic_directories_dict[os.path.dirname(filename)],
                                                 boy_irl_name)
            error_boys_dict[error_file_dir].pop(error_ig_name)

        else:
            print("That didn't work. We'll pass on this for now.")
    else:
        print("Ok, we will skip this for now.")


def print_section(section_title, symbol):
    """
    A helper method to print our output to the console in a pretty fashion

    :param section_title:   The name of the section we are printing.
    :param symbol:      The symbol to repetetively print to box in our output. This will usually be a '*'.
    :return:    None
    """
    print("\n" + (symbol * 50) + "\n" + section_title + "\n" + (symbol * 50) + "\n")


def initialize_data():
    """
    This is how we fire up our dictionary and list files that contain all the info on existing files.

    :return:    None
    """
    # Import globals
    global boys_dict
    global boys_dictionary_file
    global photographers_list_file
    global photographers_list

    # Read CSV file into a dictionary
    os.chdir(os.path.dirname(os.path.realpath(__file__)))
    with open(boys_dictionary_file, 'r', newline='') as f:
        # NOTE: newline='' means not to leave white lines between entries in the CSV file when writing new entries
        global boys_dictionary_file_field_names
        reader = csv.DictReader(f, fieldnames=boys_dict_file_field_names)
        # header = reader.fieldnames  # Advances past header so I can iterate over the dict
        next(reader)  # Skip headers
        for row in reader:
            if row["Name"] == "":
                if row["Account"] == "":
                    f.close()
                    proc = subprocess.Popen(boys_dictionary_file)
                    raise Exception("There is an empty row in " + str(
                        boys_dictionary_file) + " at line " + str(reader.line_num) + ". Please fix this.")
                else:
                    webbrowser.open("".join(["https://www.instagram.com/", row["Account"]]))
                    f.close()
                    proc = subprocess.Popen(boys_dictionary_file, shell=True)
                    raise Exception("Account \"" + str(row["Account"]) + "\" has caused an error in " + str(
                        boys_dictionary_file) + ". All account names MUST have a corresponding IRL name but this one is blank.")
            else:
                boys_dict[row["Account"]] = row["Name"]

    with open(photographers_list_file, 'r') as f:
        photographers_list = f.read().splitlines()


def name_file_to_next_available_name(full_filename, out_dir, boy_irl_name):
    # TODO: Simplify this method so instead of file_basename and in_dir we just take 1 parameter which is the complete filepath of the file to sort.
    # TODO: Fix the documentation accordingly.

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
    files_in_output_directory = os.listdir(out_dir)
    file_names_without_ext_in_output_directory = [file.split(".")[0] for file in
                                                  files_in_output_directory]  # List comprehension

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
    print(str(full_filename) + " successfully sorted to " + out_dir + " as " + new_filename_with_ext)
    global new_files_successfully_processed
    new_files_successfully_processed += 1


def print_error_boys_dict():
    """
    A helper method that prints out error_boys_dict.

    :return: None
    """
    global error_boys_dict
    for error_dir in error_boys_dict:
        print("KEY (error_dir): " + str(error_dir))
        for ig_name in error_boys_dict[error_dir]:
            print("\tIG NAME: " + ig_name)
            for fname in error_boys_dict[error_dir][ig_name]:
                print("\t\tFILE: " + fname)
    if not error_boys_dict:
        print("error_boys_dict is empty!")
    input("\nPress Enter to continue the script...")


if __name__ == '__main__':
    main()
