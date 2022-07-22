import sys

lines = sys.stdin.read().splitlines()

micromamba_files = []
existing_doc_files = []
missing_doc_files = []

# Check for changes in micromamba
for line in lines:
    line = line.split()
    if any("micromamba/" in words for words in line):
        path_words = line[-1].split("/")
        path_words = path_words[-1].split(".", 1)
        micromamba_files.append(path_words[0])

# Check if doc files were generated for micromamba
for line in lines:
    line = line.split()
    if any(
        "docs/source/user_guide/commands_micromamba/" in words for words in line
    ):
        path_words = line[-1].split("/")
        path_words = path_words[-1].split(".", 1)
        existing_doc_files.append(path_words[0])

        # Convert to tuple to eliminate possible duplicates between header and source files
        micromamba_files.sort()
        micromamba_files_tuple = tuple(file for file in micromamba_files)
        existing_doc_files.sort()
        existing_doc_files_tuple = tuple(file for file in existing_doc_files)

if micromamba_files == existing_doc_files:
    pass
# Raise error in case the doc files weren't generated
else:
    missing_doc_files = set(micromamba_files) - set(existing_doc_files)
    print("The following files require an up to date documentation:")
    print(missing_doc_files)
    print("Check the Mamba documentation about how to generate these files.")
