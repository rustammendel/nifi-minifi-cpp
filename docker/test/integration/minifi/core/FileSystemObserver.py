import logging
import time
from threading import Event

from watchdog.observers import Observer
from .OutputEventHandler import OutputEventHandler


class FileSystemObserver(object):
    def __init__(self, test_output_dir):

        self.test_output_dir = test_output_dir

        # Start observing output dir
        self.done_event = Event()
        self.event_handler = OutputEventHandler(self.done_event)
        self.observer = Observer()
        self.observer.schedule(self.event_handler, self.test_output_dir, recursive=True)
        self.observer.start()

    def get_output_dir(self):
        return self.test_output_dir

    def restart_observer_if_needed(self):
        if self.observer.is_alive():
            return

        self.observer = Observer()
        self.done_event.clear()
        self.observer.schedule(self.event_handler, self.test_output_dir, recursive=True)
        self.observer.start()

    def validate_output(self, timeout_seconds, output_validator, max_files=0):
        logging.info('Waiting up to %d seconds for valid test outputs (maximum of %d files)', timeout_seconds, max_files)
        self.restart_observer_if_needed()
        try:
            if max_files and max_files <= self.event_handler.get_num_files_created():
                return output_validator.validate()
            wait_start_time = time.perf_counter()
            while True:
                # Note: The timing on Event.wait() is inaccurate
                self.done_event.wait(timeout_seconds - time.perf_counter() + wait_start_time)
                if self.done_event.isSet():
                    self.done_event.clear()
                    if max_files and max_files <= self.event_handler.get_num_files_created():
                        return output_validator.validate()
                    if output_validator.validate():
                        return True
                if timeout_seconds < (time.perf_counter() - wait_start_time):
                    return output_validator.validate()
        finally:
            self.observer.stop()
            self.observer.join()
