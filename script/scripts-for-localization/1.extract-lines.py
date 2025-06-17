import os
import re
from collections import OrderedDict

def find_source_files(root_dir, extensions=('.cpp', '.hpp', '.h')):
    ignore_files = {'ignore1.file'}  # Игнорируемые файлы
    ignore_dirs = {'linux'}  # Игнорируемые папки
    
    print(f"\nSearch in files: {extensions}")
    print(f"Ignored files: {ignore_files}")
    print(f"Ignored folders: {ignore_dirs}")
    
    for dirpath, dirnames, filenames in os.walk(root_dir):
        # Удаляем игнорируемые папки из списка для обхода
        dirnames[:] = [d for d in dirnames if d not in ignore_dirs]
        
        for f in filenames:
            if (f.endswith(extensions) and 
                f not in ignore_files and
                not any(ignore_dir in dirpath for ignore_dir in ignore_dirs)):
                yield os.path.join(dirpath, f)

def remove_from_processing(content):
    # Убираем только настоящие комментарии, но не строки с операторами
    content = re.sub(r'/\*.*?\*/', '', content, flags=re.DOTALL)  # блочные /* */
    content = re.sub(r'^\s*//.*$', '', content, flags=re.MULTILINE)  # однострочные //
    content = re.sub(r'^\s*#.*$', '', content, flags=re.MULTILINE)  # директивы #
    return content

def is_real_text_string(s):
    # Разрешаем все строки, содержащие буквы или значимые символы
    return (
        len(s.strip()) > 0 and  # Не пустая
        not re.match(r'^\s*\d+\s*$', s) and  # Не просто число
        not s.isupper()  # Не ВСЕ заглавные
    )

def extract_strings_with_lines(file_path):
    with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
        lines = f.readlines()
    
    results = []
    for line_num, line in enumerate(lines, 1):
        # Не обрабатываем строки, которые явно являются комментариями
        if line.strip().startswith(('//', '/*', '*')):
            continue
            
        content = remove_from_processing(line)
        # Новое регулярное выражение, которое лучше находит строки в коде
        strings = re.findall(r'(?<!\\)"(?:\\"|\\[^"]|[^\\"])*?(?<!\\)"', content)
        
        for s in strings:
            s = s.strip('"')
            if is_real_text_string(s):
                results.append((s, line_num))
    
    return results

def main():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    # root_dir = os.path.join(script_dir, "original_files")
    root_dir = os.path.join(script_dir, "..", "..", "JoyShockMapper")
    all_strings = OrderedDict()

    for file_path in find_source_files(root_dir):
        try:
            strings_with_lines = extract_strings_with_lines(file_path)
            for s, line_num in strings_with_lines:
                if s not in all_strings:
                    all_strings[s] = []
                all_strings[s].append((file_path, line_num))
        except Exception as e:
            print(f"Error in {os.path.basename(file_path)}: {e}")

    output_path = os.path.join(script_dir, "extracted-lines.txt")
    log_output_path = os.path.join(script_dir, "extracted-lines_log.txt")
    
    with open(output_path, 'w', encoding='utf-8') as f:
        for s in all_strings:
            f.write(f"{s}\n")
    
    with open(log_output_path, 'w', encoding='utf-8') as f:
        for s, locations in all_strings.items():
            for file_path, line_num in locations:
                rel_path = os.path.relpath(file_path, root_dir)
                f.write(f"{s}|<-- from: {rel_path}:{line_num}\n")

    print(f"\nFound {len(all_strings)} unique lines")
    print(f"Result is saved in: \"extracted-lines.txt\"")
    print(f"Log is saved in: \"extracted-lines_log.txt\"")

if __name__ == "__main__":
    main()
    input("\nPress Enter to continue...")