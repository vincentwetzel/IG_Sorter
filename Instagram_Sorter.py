import csv
import os
import re
import sys
import webbrowser

# Directories
root_picture_directory = "E:\OneDrive\Pictures"
subdirectories_dict = {
    os.path.join(root_picture_directory, "NEED TO SORT (NSFW)"): os.path.join(root_picture_directory, "NSFW"),
    os.path.join(root_picture_directory, "NEED TO SORT (MSFW)"): os.path.join(root_picture_directory, "MSFW"),
    os.path.join(root_picture_directory, "NEED TO SORT (SFW)"): os.path.join(root_picture_directory, "SFW")}

# Data files
boys_dictionary_file = "boys.csv"  # You can call this whatever you want.
boys_dict = dict()  # { "Account":  "irl_name" }
photographers_list_file = "photographers.txt"
photographers_list = dict()

# Error handling
errors_dict = dict()  # Key = Instagram Name, Val = full_filename

# Counters
files_renamed_count = 0
new_files_successfully_processed = 0


def main():
    # Initialize variables
    global boys_dict
    global root_picture_directory
    global files_renamed_count
    global subdirectories_dict
    global new_files_successfully_processed
    global photographers_list

    initialize_data()

    # Check ordering of existing sorted files.
    for subdir in subdirectories_dict.values():
        print_section("Checking ordering in " + str(os.path.join(root_picture_directory, subdir)), "-")
        fix_numbering(os.path.join(root_picture_directory, subdir))

    # Prepping done, now sort new pics
    for subdir in subdirectories_dict:
        print_section("Sorting new pics in " + str(os.path.join(root_picture_directory, subdir)), "-")
        sort_new_pictures(boys_dict, subdir, subdirectories_dict[subdir])

    # Print errors if any exist
    if errors_dict:
        print_section("PROBLEMS", "-")
        for ig_name in list(errors_dict.keys()):
            for filename in errors_dict[ig_name]:
                print(filename)
            if ig_name in photographers_list:
                user_input = input(
                    "We have found " + str(len(errors_dict[
                                                   ig_name])) + " pictures from photograher \"" + ig_name + "\" in this batch. Are all this photographer's pitures of the same boy? (y/n)").lower()
                if user_input == "y" or user_input == "yes":
                    boy_irl_name = input(
                        "Please enter the boy's name: ")
                    if boy_irl_name in boys_dict or boy_irl_name in boys_dict.values():
                        print("I found him!")
                        for filename in errors_dict[ig_name]:
                            name_file_to_next_available_name(filename, os.path.dirname(filename),
                                                             subdirectories_dict[os.path.dirname(filename)],
                                                             boy_irl_name)
                        errors_dict.pop(ig_name)

                    else:
                        print("That didn't work. We'll pass on this for now.")
                else:
                    print("Ok, we will skip this photographer for now.")
        if "Screenshot" not in errors_dict or len(errors_dict) > 1:
            os.startfile(os.path.join(sys.path[0], boys_dictionary_file))

    # Print a final report
    print_section("FINAL REPORT", "-")
    print("NUMBER OF FILES RENAMED: " + str(files_renamed_count))
    print("NUMBER OF NEW FILES SORTED: " + str(new_files_successfully_processed))
    print("TOTAL ERRORS: " + str(sum(len(v) for v in errors_dict.values())) + "\n")

    # Open the directories with problem files
    error_directories = list()
    for path in errors_dict.values():
        if os.path.dirname(path) not in error_directories:
            error_directories.append(os.path.dirname(path))
            os.startfile(os.path.dirname(path))

    # Open the Instagram pages for problem accounts
    for ig_name in errors_dict:
        if ig_name == "Screenshot" or ig_name == "Edited Screenshot":
            continue
        print(ig_name)
        webbrowser.open("".join(["https://www.instagram.com/", ig_name]))


def sort_new_pictures(boys_dict, in_dir, out_dir):
    # Globals
    global errors_dict

    # Initialize other variables
    boy_file_name = list()
    counter = 1
    match_found = False
    next_number_for_filename = 1
    directory_not_empty_at_start = False

    # adjust current working directory (cwd)
    os.chdir(in_dir)
    if os.listdir(in_dir):
        directory_not_empty_at_start = True

    # For each file in the directory, search the CSV file for a match
    file_paths = []
    for folder, subs, files in os.walk(in_dir):
        for filename in files:
            file_paths.append(os.path.abspath(os.path.join(folder, filename)))

    for full_file_path in file_paths:
        current_file = os.path.basename(full_file_path)
        match_found = False
        boy_file_name = os.path.splitext(os.path.basename(current_file))[0]
        counter = 1
        while counter <= len(boy_file_name) and match_found is False:
            if boy_file_name[0:counter] in boys_dict.keys():
                boy_file_name = boys_dict[boy_file_name[0:counter]]
                match_found = True
            elif boy_file_name[0:counter] in boys_dict.values():
                boy_file_name = boy_file_name[0:counter]
                match_found = True
            else:
                counter += 1

        if match_found:
            # A match has been found, now keep trying until I find the right counter and rename to that.
            name_file_to_next_available_name(current_file, in_dir, out_dir, boy_file_name)
        else:
            msg = ">>>Could not process: " + str(current_file)
            print(msg)

            # Case 1: Screnshots
            # If it is a screenshot, put it under the Screenshot key in the dictionary. This will be handled in the final report.
            if "Screenshot" in boy_file_name:
                if "Screenshot" not in errors_dict:
                    errors_dict["Screenshot"] = [full_file_path]
                else:
                    errors_dict["Screenshot"].append(full_file_path)

            # Case 2: FastSave Android App
            elif "___" in boy_file_name:
                current_boy_IG_name = re.search(r".+?(?=_[0-9]*_{3,})|.+?(?=_{3,})",
                                                boy_file_name).group(0)

                if current_boy_IG_name not in errors_dict:
                    errors_dict[current_boy_IG_name] = [full_file_path]
                else:
                    errors_dict[current_boy_IG_name].append(full_file_path)
            # Case 3: Chrome Downloader for Instagram
            elif "_n" in boy_file_name:
                raise Exception("Need to implement code for pictures downloaded from Chrome.")
            # Case 4: Editied Screenshot from phone
            elif re.search(r"^[0-9]{6,8}_[0-9]{6}$", boy_file_name) is not None and str(
                    re.search(r"^[0-9]{6,8}_[0-9]{6}$", boy_file_name).group(0)) in boy_file_name:
                if "Edited Screenshot" not in errors_dict:
                    errors_dict["Edited Screenshot"] = [full_file_path]
                else:
                    errors_dict["Edited Screenshot"].append(full_file_path)
            else:
                raise Exception("EXCEPTION: Unknown file type!")

    if directory_not_empty_at_start:
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

    with open(photographers_list_file, 'r') as f:
        photographers_list = f.read().splitlines()
        print("PHOTOGRAPHERS LIST:")
        print(str(photographers_list))


def name_file_to_next_available_name(filename, in_dir, out_dir, boy_irl_name=None):
    next_number_for_filename = 1
    new_filename_without_ext = ""

    os.chdir(in_dir)
    if boy_irl_name is None:
        file_name_as_list_of_name0_and_ext1 = list(os.path.splitext(os.path.basename(filename)))
        print("file_name_as_list_of_name0_and_ext1 : " + str(file_name_as_list_of_name0_and_ext1))
    else:
        file_name_as_list_of_name0_and_ext1 = [boy_irl_name, os.path.splitext(filename)[1]]

    # If the file contains numbering already, strip that out.
    if re.search(r".+?(?=[0-9])", file_name_as_list_of_name0_and_ext1[0]) is not None:
        file_name_as_list_of_name0_and_ext1[0] = str(
            re.search(r".+?(?=[0-9])", file_name_as_list_of_name0_and_ext1[0]).group(0))

    files_in_output_directory = os.listdir(out_dir)
    file_names_in_output_directory = [file.split(".")[0] for file in
                                      files_in_output_directory]  # List comprehension

    os.chdir(out_dir)
    while True:
        new_filename_without_ext = file_name_as_list_of_name0_and_ext1[0] + " " + str(
            next_number_for_filename)
        if new_filename_without_ext in file_names_in_output_directory:
            next_number_for_filename += 1
        else:
            break
    new_filename_with_ext = new_filename_without_ext + file_name_as_list_of_name0_and_ext1[1]
    os.rename(os.path.join(in_dir, filename),
              os.path.join(out_dir, new_filename_with_ext))
    print(str(filename) + " successfully sorted to " + out_dir + " as " + new_filename_with_ext)
    global new_files_successfully_processed
    new_files_successfully_processed += 1


if __name__ == '__main__':
    main()
