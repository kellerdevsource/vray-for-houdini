if __name__ == '__main__':
    import argparse
    import os

    import dsutils

    parser = argparse.ArgumentParser()
    parser.add_argument("jsonFilePath")
    parser.add_argument("dsDirPath")

    args = parser.parse_args()

    dsutils.saveJSONAsDialogScript(args.jsonFilePath, args.dsDirPath)
