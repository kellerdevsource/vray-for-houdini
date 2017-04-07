if __name__ == '__main__':
    import argparse
    import os

    import dsutils

    parser = argparse.ArgumentParser()
    parser.add_argument("jsonDirPath")
    parser.add_argument("dsDirPath")

    args = parser.parse_args()

    for root, dirs, files in os.walk(args.jsonDirPath):
        for name in files:
            jsonFilePath = os.path.join(root, name)
            if jsonFilePath.endswith(".json"):
                dsutils.saveJSONAsDialogScript(jsonFilePath, args.dsDirPath)
