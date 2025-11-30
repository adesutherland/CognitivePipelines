"""
major_tom.py

This module simulates the communications between Ground Control and Major Tom
during the launch and subsequent orbital anomaly.

"This is Ground Control to Major Tom, you've really made the grade."
"""

import sys
import time

class GroundControl:
    """
    Represents the mission control center on Earth.
    Responsible for countdown, ignition checks, and media relations.
    """

    def __init__(self):
        self.engines_on = False
        self.comm_link_active = True

    def commence_countdown(self, start_from=10):
        """
        Initiates the launch sequence.
        Take your protein pills and put your helmet on.
        """
        print("Commencing countdown, engines on.")
        self.engines_on = True

        for i in range(start_from, 0, -1):
            # Check ignition and may God's love be with you
            print(f"{i}...")
            time.sleep(0.1)

            if i == 6:
                print("Check ignition...")
                if not self.check_ignition_systems():
                    print("ABORT!")
                    return

        print("Liftoff!")

    def check_ignition_systems(self):
        """
        Verifies that all primary thrusters are firing.
        """
        # Deep nesting test for chunker indentation logic
        if self.engines_on:
            if self.comm_link_active:
                # Signal is strong
                return True
            else:
                # We lost him
                return False
        return False

class MajorTom:
    """
    The astronaut stepping through the door.
    Floating in a most peculiar way.
    """

    def step_through_door(self):
        """
        Exits the capsule.
        """
        # And the stars look very different today
        stars_appearance = "different"

        if stars_appearance == "different":
            self.float_in_tin_can()

    def float_in_tin_can(self):
        """
        Current state: Far above the world.
        """
        planet_earth = "blue"
        nothing_i_can_do = True

        if planet_earth == "blue" and nothing_i_can_do:
            # Though I'm past one hundred thousand miles
            # I'm feeling very still
            self.disconnect_circuit()

    def disconnect_circuit(self):
        """
        Tell my wife I love her very much.
        She knows.
        """
        # Ground Control to Major Tom
        # Your circuit's dead, there's something wrong
        print("Here am I floating 'round my tin can")
        print("Far above the Moon")
        print("Planet Earth is blue")
        print("And there's nothing I can do.")

def main():
    # This is Ground Control to Major Tom
    gc = GroundControl()
    tom = MajorTom()

    gc.commence_countdown()
    tom.step_through_door()

if __name__ == "__main__":
    main()