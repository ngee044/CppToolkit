#!/usr/bin/env python3
"""
Indentation Converter Script
Converts leading spaces to tabs (4 spaces = 1 tab) in C++ files.
Processes GameDatabase and GameNetwork modules.
"""

import os
import re
import glob

def convert_spaces_to_tabs(content, tab_size=4):
    """
    Convert leading spaces to tabs in the content.
    
    Args:
        content (str): File content
        tab_size (int): Number of spaces per tab (default: 4)
    
    Returns:
        str: Content with leading spaces converted to tabs
    """
    lines = content.split('\n')
    converted_lines = []
    
    for line in lines:
        if not line.strip():  # Empty line
            converted_lines.append(line)
            continue
            
        # Count leading spaces
        leading_spaces = len(line) - len(line.lstrip(' '))
        
        if leading_spaces > 0:
            # Convert leading spaces to tabs
            tabs_count = leading_spaces // tab_size
            remaining_spaces = leading_spaces % tab_size
            
            # Build new line with tabs + remaining spaces + content
            new_line = '\t' * tabs_count + ' ' * remaining_spaces + line.lstrip(' ')
            converted_lines.append(new_line)
        else:
            # No leading spaces, keep as is
            converted_lines.append(line)
    
    return '\n'.join(converted_lines)

def process_file(file_path):
    """
    Process a single file to convert indentation.
    
    Args:
        file_path (str): Path to the file to process
    
    Returns:
        bool: True if file was modified, False otherwise
    """
    try:
        # Read file content
        with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
            original_content = f.read()
        
        # Convert indentation
        converted_content = convert_spaces_to_tabs(original_content)
        
        # Check if content changed
        if original_content != converted_content:
            # Write back the converted content
            with open(file_path, 'w', encoding='utf-8') as f:
                f.write(converted_content)
            print(f"✓ Converted: {file_path}")
            return True
        else:
            print(f"- No change: {file_path}")
            return False
            
    except Exception as e:
        print(f"✗ Error processing {file_path}: {e}")
        return False

def find_cpp_files(base_path):
    """
    Find all .cpp and .h files in the given directory.
    
    Args:
        base_path (str): Base directory to search
    
    Returns:
        list: List of file paths
    """
    cpp_files = []
    
    # Find .cpp files
    cpp_pattern = os.path.join(base_path, '**', '*.cpp')
    cpp_files.extend(glob.glob(cpp_pattern, recursive=True))
    
    # Find .h files
    h_pattern = os.path.join(base_path, '**', '*.h')
    cpp_files.extend(glob.glob(h_pattern, recursive=True))
    
    return sorted(cpp_files)

def main():
    """Main function"""
    print("Indentation Converter for GameDatabase and GameNetwork")
    print("=" * 55)
    
    base_dir = "e:/CppToolkit"
    modules = ["GameDatabase", "GameNetwork"]
    
    total_files = 0
    modified_files = 0
    
    for module in modules:
        module_path = os.path.join(base_dir, module)
        
        if not os.path.exists(module_path):
            print(f"✗ Module directory not found: {module_path}")
            continue
            
        print(f"\nProcessing module: {module}")
        print("-" * 30)
        
        files = find_cpp_files(module_path)
        
        if not files:
            print(f"No .cpp or .h files found in {module}")
            continue
            
        for file_path in files:
            total_files += 1
            if process_file(file_path):
                modified_files += 1
    
    print("\n" + "=" * 55)
    print(f"Summary:")
    print(f"Total files processed: {total_files}")
    print(f"Files modified: {modified_files}")
    print(f"Files unchanged: {total_files - modified_files}")
    
    if modified_files > 0:
        print("\n✓ Indentation conversion completed!")
        print("Please run build.bat to verify compilation.")
    else:
        print("\n- All files already have correct indentation.")

if __name__ == "__main__":
    main()
