import argparse
from concurrent.futures import as_completed, ThreadPoolExecutor
import json
import logging
import os
from pathlib import Path
import shlex
import subprocess
import sys
import time


def positive_int(value):
    try:
        parsed = int(value)
    except ValueError as error:
        raise argparse.ArgumentTypeError('must be an integer') from error
    if parsed < 1:
        raise argparse.ArgumentTypeError('must be at least 1')
    return parsed


def default_jobs():
    try:
        return max(1, len(os.sched_getaffinity(0)))
    except (AttributeError, OSError):
        return max(1, os.cpu_count() or 1)


def parse_args(argv=None):
    parser = argparse.ArgumentParser()
    parser.add_argument('-p', '--build-path', dest='build_path', required=True,
                        help='Build directory containing compile_commands.json')
    parser.add_argument('--config-file', dest='tidy_file', default='.clang-tidy')
    parser.add_argument('-j', '--jobs', type=positive_int, default=default_jobs(),
                        help='Number of clang-tidy processes to run in parallel (default: available CPUs)')
    parser.add_argument('--store-check-profile', default=None)
    parser.add_argument('files', nargs='+')
    return parser.parse_args(argv)

def is_excluded_source_path(path):
    parts = path.parts
    excluded_directories = (
        ('src', 'test'),
        ('src', 'demo'),
    )
    return any(
        parts[index:index + len(directory)] == directory
        for directory in excluded_directories
        for index in range(len(parts) - len(directory) + 1)
    )
    
def is_excluded_folder(path):
    excluded_folders = ('third_party', 'tools', 'example')
    return any(part in path.parts for part in excluded_folders)

def is_excluded_file(file):
    excluded_files = ('')
    excluded_suffixes = ('test.cc', '_benchmark.cc')
    return any(file.endswith(f) for f in excluded_files) or any(file.endswith(suffix) for suffix in excluded_suffixes)

def is_header_file(file):
    header_extensions = ('.h', '.hpp', '.hh', '.hxx')
    return any(file.endswith(ext) for ext in header_extensions)


def run_tidy(command):
    started_at = time.monotonic()
    try:
        completed = subprocess.run(command, stdout=subprocess.PIPE,
                                   stderr=subprocess.PIPE, text=True, check=False)
    except OSError as error:
        return None, time.monotonic() - started_at, error
    return completed, time.monotonic() - started_at, None


def main(argv=None):
    args = parse_args(argv)
    sources = []
    for file in args.files:
        path = Path(file)
        if is_excluded_folder(path):
            continue
        if is_excluded_source_path(path):
            continue
        if is_excluded_file(file):
            continue
        if not path.is_file():
            logging.error('Source file does not exist: %s', file)
            return 1
        sources.append(path.resolve())
    if not sources:
        logging.info('No source files to analyze')
        return 0

    build_path = Path(args.build_path).resolve()
    # Accept the old CLI form, but always pass a directory to clang-tidy.
    if build_path.name == 'compile_commands.json':
        build_path = build_path.parent
    database = build_path / 'compile_commands.json'
    try:
        entries = json.loads(database.read_text())
    except (OSError, ValueError) as error:
        logging.error('Cannot read compilation database %s: %s', database, error)
        return 1
    if not isinstance(entries, list) or not all(
            isinstance(entry, dict) and isinstance(entry.get('file'), str)
            and isinstance(entry.get('directory'), str)
            and (isinstance(entry.get('command'), str) and bool(entry['command'])
                 or isinstance(entry.get('arguments'), list) and bool(entry['arguments'])
                 and all(isinstance(arg, str) for arg in entry['arguments']))
            for entry in entries):
        logging.error('Invalid compilation database entries in %s', database)
        return 1
    compiled_sources = set()
    for entry in entries:
        path = Path(entry['file'])
        directory = Path(entry['directory'])
        if not directory.is_absolute():
            directory = build_path / directory
        compiled_sources.add((directory / path).resolve())
    missing = [
        str(path) for path in sources
        if path not in compiled_sources and not is_header_file(str(path))
    ]
    if missing:
        logging.error('Sources missing from %s: %s. Configure their CMake targets '
                      '(including -DENABLE_TEST=ON for tests) before analysis.',
                      database, ', '.join(missing))
        return 1
    headers = [path for path in sources if is_header_file(str(path))]
    if headers:
        logging.info('Analyzing %d header file(s) without direct compilation database entries',
                     len(headers))

    base_command = [
        'clang-tidy',
        f'-p={build_path}',
        f'--config-file={Path(args.tidy_file).resolve()}',
        '--quiet',
        '--warnings-as-errors=*',
    ]
    commands = []
    for source in sources:
        command = base_command.copy()
        if args.store_check_profile:
            profile_prefix = Path(args.store_check_profile)
            unique_prefix = profile_prefix.parent / f'{profile_prefix.name}.{source.name}'
            command.append(f'--store-check-profile={unique_prefix}')
        command.append(str(source))
        commands.append((source, command))
        logging.info('Started clang-tidy for %s', source)
        logging.debug('Executing clang-tidy command: %s', shlex.join(command))

    worker_count = min(args.jobs, len(commands))
    logging.info('Analyzing %d input file(s) with %d worker(s)', len(commands), worker_count)
    failures = []
    with ThreadPoolExecutor(max_workers=worker_count) as executor:
        futures = {
            executor.submit(run_tidy, command): source
            for source, command in commands
        }
        for future in as_completed(futures):
            source = futures[future]
            completed, elapsed, error = future.result()
            if error:
                logging.error('Finished clang-tidy for %s in %.2fs: execution failed: %s',
                              source, elapsed, error)
                failures.append(source)
                continue
            if completed.stdout:
                logging.info('console_output [%s]: %s', source, completed.stdout)
            if completed.stderr:
                logging.info('console_error [%s]: %s', source, completed.stderr)
            logging.info('Finished clang-tidy for %s in %.2fs (exit code %s)',
                         source, elapsed, completed.returncode)
            if completed.returncode != 0:
                logging.error('Code analysis failed for %s (exit code %s)',
                              source, completed.returncode)
                failures.append(source)

    if failures:
        logging.error('Code analysis failed for %d of %d input file(s)',
                      len(failures), len(commands))
        return 1
    logging.info('Code analysis passed')
    return 0


if __name__ == '__main__':
    logging.basicConfig(level=logging.INFO)
    sys.exit(main())
