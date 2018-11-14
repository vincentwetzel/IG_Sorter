import csv
import os
import re
import sys
import webbrowser

# Directories
root_picture_directory = "E:\OneDrive\Pictures"
subdirectories_list = ["NSFW", "MSFW", "SFW"]

# Data files
boys_dictionary_file = "boys.csv"  # You can call this whatever you want.
boys_dict = dict()
photographers_list_file = "photographers.txt"
photographers_list = dict()

# Error handling
errors_dict = dict()  # Tracks all the files that there were issues sorting
error_directories = list()  # Keep track of the directories where there were problems, open them at end of script.

# Counters
files_renamed_count = 0
new_files_successfully_processed = 0


def main():
    # Initialize variables
    global boys_dict
    global root_picture_directory
    global files_renamed_count
    global subdirectories_list
    global new_files_successfully_processed
    global photographers_list

    initialize_data()

    # Check ordering of existing sorted files.
    for subdir in subdirectories_list:
        print_section("Checking ordering in " + str(os.path.join(root_picture_directory, subdir)), "-")
        fix_numbering(os.path.join(root_picture_directory, subdir))

    # Prepping done, now sort new pics
    for subdir in subdirectories_list:
        print_section("Sorting new pics in " + str(os.path.join(root_picture_directory, subdir)), "-")
        sort_new_pictures(boys_dict,
                          os.path.join(root_picture_directory, "NEED TO SORT (" + subdir + ")"),
                          os.path.join(root_picture_directory, subdir))

    # Print errors if any exist
    if errors_dict:
        print_section("PROBLEMS", "-")
        for key in errors_dict:
            for val in errors_dict[key]:
                print(val)
            if key in photographers_list:
                user_input = input(
                    "We have found photograher " + key + " in this batch. Are all this photographer's pitures of the same boy? (y/n)").lower()
                if user_input == "y" or user_input == "yes":
                    boy = input(
                        "Please enter the boy's name: ")
                    if boy in boys_dictionary_file or boy in boys_dict.values():
                        print("I found him!")
                    else:
                        print("That didn't work. We'll pass on this for now.")
        os.startfile(os.path.join(sys.path[0], boys_dictionary_file))

    # Print a final report
    print_section("FINAL REPORT", "-")
    print("NUMBER OF FILES RENAMED: " + str(files_renamed_count))
    print("NUMBER OF NEW FILES SORTED: " + str(new_files_successfully_processed))
    print("TOTAL ERRORS: " + str(sum(len(v) for v in errors_dict.values())) + "\n")

    # Open the directories with problem files
    for directory in error_directories:
        os.startfile(directory)

    # Open the Instagram pages for problem accounts
    for key in errors_dict:
        if key == "Screenshot":
            continue
        print(key)
        webbrowser.open("".join(["https://www.instagram.com/", key]))


def sort_new_pictures(boys_dict, in_dir, out_dir):
    # Globals
    global errors_dict
    global error_directories

    # Initialize other variables
    file_name_as_list_of_name0_and_ext1 = list()
    counter = 1
    match_found = False
    next_number_for_filename = 1
    new_files_exist = False

    # adjust current working directory (cwd)
    os.chdir(in_dir)
    if os.listdir(in_dir):
        new_files_exist = True

    # For each file in the directory, search the CSV file for a match
    for current_file in os.listdir(in_dir):
        match_found = False
        file_name_as_list_of_name0_and_ext1 = list(os.path.splitext(os.path.basename(current_file)))
        counter = 1
        while counter <= len(file_name_as_list_of_name0_and_ext1[0]) and match_found is False:
            if file_name_as_list_of_name0_and_ext1[0][0:counter] in boys_dict.keys():
                file_name_as_list_of_name0_and_ext1[0] = boys_dict[file_name_as_list_of_name0_and_ext1[0][0:counter]]
                match_found = True
            elif file_name_as_list_of_name0_and_ext1[0][0:counter] in boys_dict.values():
                file_name_as_list_of_name0_and_ext1[0] = file_name_as_list_of_name0_and_ext1[0][0:counter]
                match_found = True
            else:
                counter += 1

        if match_found:
            # A match has been found, now keep trying until I find the right counter and rename to that.
            name_file_to_next_available_name(current_file, in_dir, out_dir)
        else:
            msg = ">>>Could not process: " + str(current_file)
            print(msg)

            # Case 1: Screnshots
            # If it is a screenshot, put it under the Screenshot key in the dictionary. This will be handled in the final report.
            if "Screenshot" in file_name_as_list_of_name0_and_ext1[0]:
                if "Screenshot" not in errors_dict:
                    errors_dict["Screenshot"] = [file_name_as_list_of_name0_and_ext1[0]]
                else:
                    errors_dict["Screenshot"].append(file_name_as_list_of_name0_and_ext1[0])

            # Case 2: FastSave Android App
            elif "___" in file_name_as_list_of_name0_and_ext1[0]:
                current_boy_containing_potential_numbering = re.search(r".+?(?=_{3,})",
                                                                       file_name_as_list_of_name0_and_ext1[0]).group(0)
                print("current_boy_containing_potential_numbering: " + current_boy_containing_potential_numbering)

                # Strip the numbers out if needed
                if re.search(r".+?(?=_[0-9])", current_boy_containing_potential_numbering) is not None:
                    current_boy_raw_IG_name = re.search(r".+?(?=_[0-9])",
                                                        current_boy_containing_potential_numbering).group(0)
                else:
                    current_boy_raw_IG_name = current_boy_containing_potential_numbering

                if current_boy_raw_IG_name not in errors_dict:
                    errors_dict[current_boy_raw_IG_name] = [current_file]
                else:
                    errors_dict[current_boy_raw_IG_name].append(current_file)
            # Case 3: Chrome Downloader for Instagram
            elif "_n" in file_name_as_list_of_name0_and_ext1[0]:
                raise Exception("Need to implement code for pictures downloaded from Chrome.")
            else:
                raise Exception(
                    "The file " + str(current_file) + " is not in a compatible format to sort with this script.")

            # Keep a list of the problem directories
            if in_dir not in error_directories:
                error_directories.append(in_dir)
    if new_files_exist:
        print()
    print("Done sorting from " + str(in_dir) + " to " + str(out_dir) + ".")


def fix_numbering(dir):
    # Initialize variables
    previous_boy_name = None
    current_boy_name = ""
    current_pic_counter = -1  # How many pictures of this boy?
    max_pic_counter = 1
    problems_exist = False

    current_boy_list = [None]
    file_name_as_list_of_name0_and_ext1 = list()

    # Get all the shit in my current working directory
    os.chdir(dir)  # changes current working directory
    files_list = os.listdir(os.getcwd())
    if os.path.isfile(os.path.join(dir, "Thumbs.db")):
        files_list.remove("Thumbs.db")

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
                        os.rename(dir + "\\" + picture_of_current_boy, dir + "\\" + new_file_name_and_ext)
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
    print("Done fixing numbering in " + str(dir) + ".")


def print_section(section_title, symbol):
    print("\n" + (symbol * 50) + "\n" + section_title + "\n" + (symbol * 50) + "\n")


def initialize_data():
    # Import globals
    global boys_dict
    global boys_dictionary_file
    global photographers_list_file
    global photographers_list

    # Read CSV file into a dictionary
    with open(boys_dictionary_file, 'r', newline='') as f:
        # NOTE: newline='' means not to leave white lines between entries in the CSV file when writing new entries
        field_names = ["Account", "Name"]  # used by writerow()
        reader = csv.DictReader(f, fieldnames=field_names)
        # header = reader.fieldnames  # Advances past header so I can iterate over the dict
        next(reader)  # Skip headers
        for row in reader:
            boys_dict[row["Account"]] = row["Name"]

    with open(photographers_list_file, 'r', newline='') as f:
        photographers_list = f.readlines()


def name_file_to_next_available_name(filename, in_dir, out_dir):
    next_number_for_filename = 1
    new_filename_without_ext = ""

    os.chdir(in_dir)
    file_name_as_list_of_name0_and_ext1 = list(os.path.splitext(os.path.basename(filename)))

    # If the file contains numbering already, strip that out.
    if re.search(r".+?(?=[0-9])", file_name_as_list_of_name0_and_ext1[0]) is not None:
        file_name_as_list_of_name0_and_ext1[0] = str(
            re.search(r".+?(?=[0-9])", file_name_as_list_of_name0_and_ext1[0]).group(0))

    files_in_output_directory = os.listdir(out_dir)
    file_names_in_output_directory = [file.split(".")[0] for file in
                                      files_in_output_directory]  # List comprehension

    os.chdir(out_dir)
    while True:
        new_filename_without_ext = file_name_as_list_of_name0_and_ext1[0] + str(
            next_number_for_filename)
        if new_filename_without_ext in file_names_in_output_directory:
            next_number_for_filename += 1
        else:
            break
    new_filename_with_ext = new_filename_without_ext + file_name_as_list_of_name0_and_ext1[1]
    os.rename(os.path.join(in_dir, filename),
              os.path.join(out_dir, new_filename_with_ext))
    print(str(filename) + " successfully sorted to " + out_dir + " as " + new_filename_without_ext)
    global new_files_successfully_processed
    new_files_successfully_processed += 1


if __name__ == '__main__':
    main()
