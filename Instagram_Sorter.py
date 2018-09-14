import csv
import os
import re


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

    # Prepping done, let's sort baby!
    # print("-" * 50)
    print("-" * 50 + "\nSorting NSFW\n" + "-" * 50)
    sorting_function(boys_dict, root_picture_directory + "\\NEED TO SORT (NSFW)", root_picture_directory + "\\NSFW")
    print("-" * 50 + "\nSorting SFW\n" + "-" * 50)
    sorting_function(boys_dict, root_picture_directory + "\\NEED TO SORT (SFW)", root_picture_directory + "\\SFW")


def sorting_function(boys_dict, in_dir, out_dir):
    # Initialize variables
    file_name_as_list_of_name0_and_ext1 = ""
    counter = 1
    match_found = False
    next_number_for_filename = 1

    boy_names = fix_numbering_for_boys_in_dir(out_dir)

    # adjust current working directory (cwd)
    os.chdir(in_dir)

    # For each file in the directory, search the CSV file for a match
    for current_file in os.listdir(os.getcwd()):
        match_found = False
        file_name_as_list_of_name0_and_ext1 = list(os.path.splitext(os.path.basename(current_file)))
        counter = 1
        while counter <= len(file_name_as_list_of_name0_and_ext1[0]) and match_found is False:
            if file_name_as_list_of_name0_and_ext1[0][0:counter] in boys_dict.keys() or \
                    file_name_as_list_of_name0_and_ext1[0][0:counter] in boys_dict.values():
                file_name_as_list_of_name0_and_ext1[0] = boys_dict[file_name_as_list_of_name0_and_ext1[0][0:counter]]
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
            print("Could not process: " + str(current_file))
    print("done")


def fix_numbering_for_boys_in_dir(dir):
    # Initialize variables
    previous_boy_name = None
    current_boy_name = ""
    current_pic_counter = -1  # How many pictures of this boy?
    max_pic_counter = 1

    current_boy_list = [None] * 1000    # TODO: Figure out how to make this the exact size I want, also at the end of code
    file_name_as_list_of_name0_and_ext1 = list()

    # Get all the shit in my current working directory
    os.chdir(dir)  # changes current working directory
    files_list = os.listdir(os.getcwd())
    files_list.remove("Thumbs.db")

    # Compile files_list into boy_names_and_numbers_list_of_lists, we will sort it in a minute
    for current_file in files_list:
        file_name_as_list_of_name0_and_ext1 = list(os.path.splitext(os.path.basename(current_file)))

        current_boy_name = re.search(r"[^0-9]+", file_name_as_list_of_name0_and_ext1[0]).group().rstrip()
        current_pic_counter = int(re.search(r"[0-9]+", file_name_as_list_of_name0_and_ext1[0]).group())  # pic_counter
        # print("Currently Processing: " + current_boy_name + " " + str(current_pic_counter))

        if previous_boy_name == current_boy_name:
            current_boy_list.insert(current_pic_counter, current_file)
            current_boy_list.pop(current_pic_counter + 1)
            # print("Currently Processing: " + current_boy_name + " " + str(current_pic_counter) + ", inserted at position: " + str(current_pic_counter))
            if current_pic_counter > max_pic_counter:
                max_pic_counter = current_pic_counter
                # print("max_pic_counter incremented to:" + str(max_pic_counter))
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
                    # print(">Inner loop: Currently processing: " + inner_current_boy_name + " " + str(inner_current_pic_counter))

                    if inner_current_pic_counter != counter_should_be:
                        print(">>NUMBERING PROBLEM: Currently processing: " + inner_current_boy_name + " " + str(
                            inner_current_pic_counter))
                        new_file_name_and_ext = inner_current_boy_name + " " + str(counter_should_be) + str(
                            pic_file_name_as_list_of_name0_and_ext1[1])
                        os.rename(dir + "\\" + picture_of_current_boy, dir + "\\" + new_file_name_and_ext)
                        print("File: " + str(picture_of_current_boy) + " renamed to: " + new_file_name_and_ext)

                    counter_should_be += 1
                    # Then reset the list to empty and put the first element in it.
                else:
                    pass
                    # print("None value at position " + str(loop_counter))
                loop_counter += 1

            max_pic_counter = current_pic_counter
            current_boy_list = [None] * 1000 # reset list
            current_boy_list.insert(current_pic_counter, current_file)
            current_boy_list.pop(current_pic_counter + 1)
            # print("Currently Processing: " + current_boy_name + " " + str(current_pic_counter) + ", inserted at position: " + str(current_pic_counter))
        previous_boy_name = current_boy_name


if __name__ == '__main__':
    main()
