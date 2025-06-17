JoyShockMapper Localization Guide Using Scripts  
Preparation  
    Ensure Python is installed (I have v3.13)  

Step 1: Extracting Strings for Translation  
    Run the script 1.extract-lines.py  
        python 1.extract-lines.py  

    The script will create two files:  
        extracted-lines.txt - contains all found strings requiring translation  
        extracted-lines_log.txt - contains information about the origin of each string (file and line number)  

Step 2: Preparing the Translation File  
    Open extracted-lines.txt in a text editor  

    Carefully review all strings and remove:  
        Incorrectly added strings  
        Hardcoded values that do not require translation  

    Save the edited file as translation-en.txt  

Step 3: Translating Strings  
    Create a copy of translation-en.txt named translation-xx.txt, where xx is the language code (e.g., "ru" for Russian)  

    Translate all strings in translation-xx.txt while maintaining:  
        The original order of strings  
        The exact formatting  
        The same number of lines in both files  

Step 4: Synchronizing Spaces  
    Open the script 2.sync-spaces-in-lines.py in a text editor  

    Modify the function call line to specify your files:  
        sync_spaces('translation-en.txt', 'translation-xx.txt')  

    Run the script:  
        python 2.sync-spaces-in-lines.py  

    The script will automatically adjust leading and trailing spaces in the translated strings to match the original  

Step 5: Applying the Translation  
    Open the script 3.translation.py in a text editor  

    Modify the translated filename variable:  
        translated_file = os.path.join(script_dir, 'translation-xx.txt')  

    Run the script:  
        python 3.translation.py  

    The script will:  
        Create backups of the original files  
        Apply the translation to all relevant files  
        Save translated files in the translated_files folder  
        Generate translation_log.txt listing all replacements and unused strings  

Completion  
    Manually move all translated files from the translated_files folder into the JoyShockMapper source code folder, replacing the original files  

    Build the project as usual  