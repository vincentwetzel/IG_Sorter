import csv
import os
import re


def main():
    boys_dict = dict()
    picture_dir = "E:\OneDrive\Pictures"

    with open("names.csv", 'r', newline='') as f:
        # NOTE: newline='' means not to leave white lines between entries in the CSV file when writing new entries
        field_names = ["Account", "Name"]  # used by writerow()
        reader = csv.DictReader(f, fieldnames=field_names)
        # header = reader.fieldnames  # Advances past header so I can iterate over the dict
        next(reader)  # Skip headers
        for row in reader:
            # print("ROW:" + str(row))
            boys_dict[row["Account"]] = row["Name"]
    cwd = picture_dir
    # print("-" * 50)
    print("-" * 50 + "\nSorting NSFW\n" + "-" * 50)
    sorting_function(boys_dict, cwd + "\\NEED TO SORT (NSFW)", cwd + "\\NSFW")
    print("-" * 50 + "\nSorting SFW\n" + "-" * 50)
    sorting_function(boys_dict, cwd + "\\NEED TO SORT (SFW)", cwd + "\\SFW")


def sorting_function(boys_dict, in_dir, out_dir):
    # For each file in the directory
    os.chdir(in_dir)
    file_as_list = ""
    counter = 1
    match_found = False
    next_number_for_filename = 1
    for file in os.listdir(os.getcwd()):
        match_found = False
        file_as_list = list(os.path.splitext(os.path.basename(file)))
        counter = 1
        while counter <= len(file_as_list[0]) and match_found is False:
            if file_as_list[0][0:counter] in boys_dict.keys():
                file_as_list[0] = boys_dict[file_as_list[0][0:counter]]
                match_found = True
            elif file_as_list[0][0:counter] in boys_dict.values():
                file_as_list[0] = file_as_list[0][0:counter]
                match_found = True
            else:
                counter += 1

        if match_found:

            # A match has been found, now keep trying until I find the right counter and rename to that.
            next_number_for_filename = 1
            new_filename = ""

            files_in_output_directory = os.listdir(out_dir)
            file_names_in_output_directory = [file.split(".")[0] for file in
                                              files_in_output_directory]  # List comprehension

            os.chdir(out_dir)
            while True:
                new_filename = str(file_as_list[0]) + " " + str(next_number_for_filename)
                if new_filename in file_names_in_output_directory:
                    next_number_for_filename += 1
                else:
                    break
            os.rename(in_dir + "\\" + file, out_dir + "\\" + new_filename + "." + str(file_as_list[1]))
            print(str(file) + " successfully sorted to " + out_dir + " as " + new_filename)
        else:
            print("Could not process: " + str(file))
    print("done")


if __name__ == '__main__':
    main()
