import csv
import os
import re

errors_list = list()
error_directories = list()


def main():
    # Initialize variables
    boys_dict = dict()
    root_picture_directory = "E:\OneDrive\Pictures"

    # Read CSV file into a dictionary
    with open("names.csv", 'r', newline='') as f:
        # NOTE: newline='' means not to leave white lines between entries in the CSV file when writing new entries
        field_names = ["Account", "Name"]  # used by writerow()
        reader = csv.DictReader(f, fieldnames=field_names)
        # header = reader.fieldnames  # Advances past header so I can iterate over the dict
        next(reader)  # Skip headers
        for row in reader:
            # print("ROW:" + str(row))
            boys_dict[row["Account"]] = row["Name"]

    print_section("Checking ordering in " + str(root_picture_directory) + "\\NSFW", "-")
    fix_numbering_for_boys_in_dir(root_picture_directory + "\\NSFW")
    print_section("Checking ordering in " + str(root_picture_directory) + "\\MSFW", "-")
    fix_numbering_for_boys_in_dir(root_picture_directory + "\\MSFW")
    print_section("Checking ordering in " + str(root_picture_directory) + "\\SFW", "-")
    fix_numbering_for_boys_in_dir(root_picture_directory + "\\SFW")

    # Prepping done, now sort new pics
    print_section("Sorting new pics in " + str(root_picture_directory) + "\\NSFW", "-")
    sorting_function(boys_dict, root_picture_directory + "\\NEED TO SORT (NSFW)", root_picture_directory + "\\NSFW")
    print_section("Sorting new pics in " + str(root_picture_directory) + "\\MSFW", "-")
    sorting_function(boys_dict, root_picture_directory + "\\NEED TO SORT (MSFW)", root_picture_directory + "\\MSFW")
    print_section("Sorting new pics in " + str(root_picture_directory) + "\\SFW", "-")
    sorting_function(boys_dict, root_picture_directory + "\\NEED TO SORT (SFW)", root_picture_directory + "\\SFW")

    print("\n\nTOTAL ERRORS: " + str(len(errors_list)) + "\n")
    if len(errors_list) > 0:
        for error in errors_list:
            print(error)
        for directory in error_directories:
            os.startfile(directory)


def sorting_function(boys_dict, in_dir, out_dir):
    # Initialize variables
    file_name_as_list_of_name0_and_ext1 = ""
    counter = 1
    match_found = False
    next_number_for_filename = 1

    # adjust current working directory (cwd)
    os.chdir(in_dir)

    # For each file in the directory, search the CSV file for a match
    for current_file in os.listdir(os.getcwd()):
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

            next_number_for_filename = 1
            new_filename_without_ext = ""

            files_in_output_directory = os.listdir(out_dir)
            file_names_in_output_directory = [file.split(".")[0] for file in
                                              files_in_output_directory]  # List comprehension

            os.chdir(out_dir)
            while True:
                new_filename_without_ext = str(file_name_as_list_of_name0_and_ext1[0]) + " " + str(
                    next_number_for_filename)
                if new_filename_without_ext in file_names_in_output_directory:
                    next_number_for_filename += 1
                else:
                    break
            os.rename(in_dir + "\\" + current_file,
                      out_dir + "\\" + new_filename_without_ext + "." + str(file_name_as_list_of_name0_and_ext1[1]))
            print(str(current_file) + " successfully sorted to " + out_dir + " as " + new_filename_without_ext)
        else:
            msg = ">>>Could not process: " + str(current_file)
            print(msg)
            global errors_list
            global error_directories
            errors_list.append(msg + " in " + in_dir)
            if in_dir not in error_directories:
                error_directories.append(in_dir)
    print("Done sorting from " + str(in_dir) + " to " + str(out_dir) + ".")


def fix_numbering_for_boys_in_dir(dir):
    # Initialize variables
    previous_boy_name = None
    current_boy_name = ""
    current_pic_counter = -1  # How many pictures of this boy?
    max_pic_counter = 1

    current_boy_list = [None]
    file_name_as_list_of_name0_and_ext1 = list()

    # Get all the shit in my current working directory
    os.chdir(dir)  # changes current working directory
    files_list = os.listdir(os.getcwd())
    try:
        files_list.remove("Thumbs.db")
    except ValueError:
        print(">>>ValueError Exception: Thumbs.db does not exist in " + str(dir))

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
            # print("About to loop, max_pic_counter is currently: " + str(max_pic_counter))
            for picture_of_current_boy in current_boy_list[0:max_pic_counter + 1]:
                # print("loop counter: " + str(loop_counter))
                if picture_of_current_boy:
                    # Initialize variables
                    pic_file_name_as_list_of_name0_and_ext1 = list(
                        os.path.splitext(os.path.basename(picture_of_current_boy)))
                    inner_current_boy_name = re.search(r"[^0-9]+",
                                                       pic_file_name_as_list_of_name0_and_ext1[0]).group().rstrip()
                    inner_current_pic_counter = int(
                        re.search(r"[0-9]+", pic_file_name_as_list_of_name0_and_ext1[0]).group())  # pic_counter

                    if inner_current_pic_counter != counter_should_be:
                        print(">>NUMBERING PROBLEM: Currently processing: " + inner_current_boy_name + " " + str(
                            inner_current_pic_counter))
                        new_file_name_and_ext = inner_current_boy_name + " " + str(counter_should_be) + str(
                            pic_file_name_as_list_of_name0_and_ext1[1])
                        os.rename(dir + "\\" + picture_of_current_boy, dir + "\\" + new_file_name_and_ext)
                        print("File: " + str(picture_of_current_boy) + " renamed to: " + new_file_name_and_ext)

                    counter_should_be += 1
                loop_counter += 1

            max_pic_counter = current_pic_counter
            current_boy_list = [None]  # reset list
            if max_pic_counter > len(current_boy_list) + 1:
                current_boy_list[len(current_boy_list): max_pic_counter + 1] = [None] * (
                        max_pic_counter - len(current_boy_list))
            current_boy_list.insert(current_pic_counter, current_file)
        previous_boy_name = current_boy_name
        # print("current_boy_list: " + str(current_boy_list))
    print("Done fixing numbering in " + str(dir) + ".")


def print_section(title, symbol):
    print("\n" + (symbol * 50) + "\n" + title + "\n" + (symbol * 50) + "\n")


if __name__ == '__main__':
    main()
