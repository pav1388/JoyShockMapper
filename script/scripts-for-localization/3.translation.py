import os
import re
from collections import defaultdict
from datetime import datetime
import shutil

def load_translation_dictionary(original_file, translated_file):
    """Загружает словарь переводов (строки должны быть без кавычек)"""
    with open(original_file, 'r', encoding='utf-8') as f_orig, \
         open(translated_file, 'r', encoding='utf-8-sig') as f_trans:
        originals = [line.rstrip('\n') for line in f_orig]
        translations = [line.rstrip('\n') for line in f_trans]
    
    if len(originals) != len(translations):
        print("Error: Translation files have different number of lines")
        return None
    
    return dict(zip(originals, translations))

def should_process_file(filepath):
    """Определяет, нужно ли обрабатывать файл"""
    if 'linux' in filepath.split(os.sep):
        return False
    
    ext = os.path.splitext(filepath)[1].lower()
    return ext in ('.cpp', '.hpp', '.h')

def write_with_bom(filepath, content):
    """Записывает файл с UTF-8 BOM"""
    with open(filepath, 'wb') as f:
        f.write(b'\xEF\xBB\xBF')  # UTF-8 BOM
        f.write(content.encode('utf-8'))

def backup_original_file(input_path, input_dir, backup_subdir):
    """Создает резервную копию оригинального файла с сохранением структуры папок"""
    # Получаем относительный путь от исходной директории
    relative_path = os.path.relpath(input_path, input_dir)
    
    # Формируем полный путь для бэкапа
    backup_path = os.path.join(backup_subdir, relative_path)
    
    # Создаем все необходимые поддиректории
    os.makedirs(os.path.dirname(backup_path), exist_ok=True)
    
    # Копируем файл
    shutil.copy2(input_path, backup_path)
    
    return backup_path

def process_file(input_path, output_path, translation_dict, log, input_dir, backup_subdir):
    """Обрабатывает один файл, ища текст в кавычках"""
    with open(input_path, 'r', encoding='utf-8-sig') as f_in:
        lines = f_in.readlines()
        content = ''.join(lines)
    
    modified_content = content
    replacements = []
    
    # Ищем все строки в кавычках (с обработкой экранированных кавычек)
    string_matches = list(re.finditer(r'(?<!\\)"(?:\\"|\\\\|\\[^"]|[^\\"])*?(?<!\\)"', content))
    
    for match in reversed(string_matches):  # Обрабатываем с конца
        quoted_str = match.group(0)
        unquoted_str = quoted_str[1:-1]  # Убираем обрамляющие кавычки
        
        # Ищем в словаре переводов (без кавычек)
        if unquoted_str in translation_dict:
            # Формируем строку с теми же кавычками и пробелами
            new_quoted_str = f'"{translation_dict[unquoted_str]}"'
            modified_content = modified_content[:match.start()] + new_quoted_str + modified_content[match.end():]
            
            # Определяем номер строки
            line_number = content.count('\n', 0, match.start()) + 1
            replacements.append((match.start(), quoted_str, new_quoted_str, line_number))
    
    if replacements:
        # Создаем резервную копию перед изменением
        backup_path = backup_original_file(input_path, input_dir, backup_subdir)
        log.write(f"\nBackup created: {backup_path}\n")
        
        write_with_bom(output_path, modified_content)
        log.write(f"\nFile: {input_path}\n")
        for pos, orig, repl, line_num in sorted(replacements, key=lambda x: x[0]):
            log.write(f"Replaced line {line_num}: {orig} --> {repl}\n")
        return len(replacements)
    return 0

def process_directory(input_dir, output_dir, translation_dict, log_file, backup_dir):
    """Рекурсивно обрабатывает все файлы в директории"""
    used_strings = set()
    unused_strings = set(translation_dict.keys())
    processed_files = 0
    total_replacements = 0

    # Создаем подпапку для бэкапов один раз в начале обработки
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    backup_subdir = os.path.join(backup_dir, timestamp)
    os.makedirs(backup_subdir, exist_ok=True)

    with open(log_file, 'w', encoding='utf-8-sig') as log:
        log.write("\xEF\xBB\xBF")  # BOM для лог-файла
        
        for root, dirs, files in os.walk(input_dir):
            if 'linux' in dirs:
                dirs.remove('linux')
            
            for filename in files:
                input_path = os.path.join(root, filename)
                
                if not should_process_file(input_path):
                    continue
                
                relative_path = os.path.relpath(root, input_dir)
                output_path_dir = os.path.join(output_dir, relative_path)
                os.makedirs(output_path_dir, exist_ok=True)
                output_path = os.path.join(output_path_dir, filename)
                
                replacements = process_file(input_path, output_path, translation_dict, log, input_dir, backup_subdir)
                
                if replacements > 0:
                    processed_files += 1
                    total_replacements += replacements
                    # Помечаем использованные строки
                    with open(input_path, 'r', encoding='utf-8-sig') as f:
                        content = f.read()
                        for orig in translation_dict:
                            if f'"{orig}"' in content:
                                used_strings.add(orig)
        
        # Определяем неиспользованные строки
        unused_strings -= used_strings
        unused_count = len(unused_strings)
        
        # Логируем неиспользованные строки
        if unused_count > 0:
            log.write("\n\nUnused translation lines:\n")
            log.write("=" * 80 + "\n")
            for unused in sorted(unused_strings):
                log.write(f"{unused}\n")
    
    return processed_files, total_replacements, unused_count

def main():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    original_file = os.path.join(script_dir, 'translation-en.txt')  # Файл с оригинальными строками
    translated_file = os.path.join(script_dir, 'translation-ru.txt')  # Файл с переводами
    input_directory = os.path.join(script_dir, "..", "..", "JoyShockMapper")  # Директория с файлами для обработки
    backup_directory = os.path.join(script_dir, 'backup_files')  # Директория с резервной копией оригинальных файлов
    output_directory = os.path.join(script_dir, 'translated_files')  # Директория для результатов
    log_file_path = os.path.join(script_dir, 'translation_log.txt')  # Файл лога
    
    print("\nLoading translation dictionary...")
    translation_dict = load_translation_dictionary(original_file, translated_file)
    if not translation_dict:
        return
    
    # Создаем основную директорию для бэкапов, если ее нет
    os.makedirs(backup_directory, exist_ok=True)
    
    print("File processing...")
    processed_count, replacements_count, unused_count = process_directory(
        input_directory, output_directory, translation_dict, log_file_path, backup_directory)
    
    print(f"\nProcessing completed successfully.")
    print(f"- Files processed: {processed_count}")
    print(f"- Total replacements: {replacements_count}")
    print(f"- Unused translation lines: {unused_count - 1}")
    print(f"- Log is saved in: {log_file_path}")
    print(f"- Results is saved in: {output_directory} (all files UTF-8 with BOM for VS)")
    print(f"- Original files backup is saved in: {os.path.join(backup_directory, datetime.now().strftime('%Y%m%d_%H%M%S'))}")

if __name__ == "__main__":
    main()
    input("\nPress Enter to continue...")