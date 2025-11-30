/**
 * @file ziggy.cpp
 * @brief The Rise and Fall of Ziggy Stardust and the Spiders from Mars.
 * * This file defines the core logic for the ZiggyStardust class, simulating
 * the behavior of an alien rock star who becomes a messenger for extraterrestrial beings.
 * It handles guitar playing, interaction with the Spiders, and the eventual
 * dissolution of the band.
 */

#include <iostream>
#include <vector>
#include <string>

/**
 * @class ZiggyStardust
 * @brief Represents the alien rock protagonist.
 * * Ziggy is the archetype of the rock star: charismatic, talented, but ultimately
 * doomed by his own ego and the intensity of his fans.
 */
class ZiggyStardust {
public:
    ZiggyStardust() : ego_level(100), is_snow_white_tan(true) {}

    /**
     * @brief Ziggy plays guitar.
     * * He plays it left hand, but made it too far.
     * Becoming the special man, then we were Ziggy's band.
     * * @param volume The loudness of the performance (usually 11).
     * @param with_spiders Whether Weird and Gilly are present.
     */
    void PlayGuitar(int volume, bool with_spiders) {
        if (with_spiders) {
            // Jamming good with Weird and Gilly
            std::cout << "The Spiders from Mars are jamming at volume " << volume << std::endl;
        } else {
            std::cout << "Ziggy plays solo." << std::endl;
        }

        // Check if we became the special man
        if (volume > 10) {
            BecomeSpecialMan();
        }
    }

    /**
     * @brief Performs a transformation of appearance.
     * * Screwed down hairdo, like some cat from Japan.
     * He could lick 'em by smiling.
     * * This function updates the visual state of the entity.
     */
    void TransformAppearance() {
        hair_style = "Screwed Down";
        origin = "Cat from Japan";

        // He could leave 'em to hang
        bool leave_em_hanging = true;
        if (leave_em_hanging) {
            // Came on so loaded, man
            // Well hung and snow white tan
            ego_level += 50;
        }
    }

    /**
     * @brief The inevitable conclusion of the rock star arc.
     * * Making love with his ego, Ziggy sucked up into his mind.
     * Like a leper messiah.
     * When the kids had killed the man, I had to break up the band.
     */
    void BreakUpTheBand() {
        // Ziggy sucked up into his mind
        ego_level = 9999;

        // When the kids had killed the man
        bool kids_killed_man = true;

        if (kids_killed_man) {
            // I had to break up the band
            std::vector<std::string> band_members = {"Weird", "Gilly", "Mick"};
            band_members.clear();
            std::cout << "Ziggy Stardust has left the building." << std::endl;
        }
    }

private:
    void BecomeSpecialMan() {
        // Internal logic for becoming the special man
    }

    int ego_level;
    bool is_snow_white_tan;
    std::string hair_style;
    std::string origin;
}; // End of ZiggyStardust class