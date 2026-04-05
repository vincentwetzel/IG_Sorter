import os
import configparser


def main():
    config = configparser.ConfigParser()
    config.read('settings.ini')
    root_picture_dir = config.get('Paths', 'root_picture_dir', fallback="I:/Google Drive (radagastthe3rd@gmail.com)/Pictures/")

    res_sfw = search_directory_and_print_results(os.path.join(root_picture_dir, "SFW"))
    res_msfw = search_directory_and_print_results(os.path.join(root_picture_dir, "MSFW"))
    res_nsfw = search_directory_and_print_results(os.path.join(root_picture_dir, "NSFW"))

    print("SFW: " + str(res_sfw))
    print("MSFW: " + str(res_msfw))
    print("NSFW: " + str(res_nsfw))


def find_files_same_size(directory):
    """
    Finds files with the same byte size within a directory.

    Args:
        directory (str): The path to the directory to search.

    Returns:
        dict: A dictionary where keys are file sizes (in bytes) and values are lists of file paths
              with that size. Returns an empty dictionary if no files are found or an error occurs.
    """
    file_sizes = {}
    try:
        for filename in os.listdir(directory):
            filepath = os.path.join(directory, filename)
            if os.path.isfile(filepath):
                file_size = os.path.getsize(filepath)
                if file_size in file_sizes:
                    file_sizes[file_size].append(filepath)
                else:
                    file_sizes[file_size] = [filepath]
    except OSError as e:
        print(f"Error accessing directory: {e}")
        return {}

    result = {size: files for size, files in file_sizes.items() if len(files) > 1}
    return result


def search_directory_and_print_results(directory_to_search: str):
    """
    TODO: Docstring
    :param directory_to_search:
    :return:
    """

    files_same_size = find_files_same_size(directory_to_search)
    count = 0;

    if files_same_size:
        for size, files in files_same_size.items():
            if os.path.basename(files[0])[0:5] != os.path.basename(files[1])[0:5]:
                continue

            print(f"Files with size {size} bytes:")
            for file_path in files:
                print(f"- {file_path}")
            count += 1
        print("TOTAL ITEMS FOUND: " + str(count))
    else:
        print("No files with the same size found in the directory.")
    return count


# Example usage:
if __name__ == "__main__":
    main()
