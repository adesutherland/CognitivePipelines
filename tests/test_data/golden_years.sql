-- Golden Years / Sound and Vision Discography Database
-- Ch-ch-ch-ch-changes: Turn and face the strange

CREATE TABLE albums (
                        id INT PRIMARY KEY,
                        title VARCHAR(255) NOT NULL,
                        year INT,
                        persona VARCHAR(100) -- E.g., Ziggy, Thin White Duke
);

CREATE TABLE tracks (
                        id INT PRIMARY KEY,
                        album_id INT,
                        title VARCHAR(255),
                        duration_seconds INT,
                        is_instrumental BOOLEAN DEFAULT FALSE,
                        FOREIGN KEY (album_id) REFERENCES albums(id)
);

-- Create the index for faster lookup of the golden years
CREATE INDEX idx_album_year ON albums(year);

-- Insert Albums (The Golden Years)
INSERT INTO albums (id, title, year, persona) VALUES (1, 'The Rise and Fall of Ziggy Stardust', 1972, 'Ziggy Stardust');
INSERT INTO albums (id, title, year, persona) VALUES (2, 'Aladdin Sane', 1973, 'Aladdin Sane');
INSERT INTO albums (id, title, year, persona) VALUES (3, 'Station to Station', 1976, 'Thin White Duke');
INSERT INTO albums (id, title, year, persona) VALUES (4, 'Low', 1977, 'The Berliner');
INSERT INTO albums (id, title, year, persona) VALUES (5, 'Heroes', 1977, 'The Berliner');
INSERT INTO albums (id, title, year, persona) VALUES (6, 'Scary Monsters (and Super Creeps)', 1980, 'The Clown');

-- Insert Tracks (Sound and Vision)
-- Don't let me hear you say life's taking you nowhere, angel
INSERT INTO tracks (id, album_id, title, duration_seconds) VALUES (1, 1, 'Five Years', 282);
INSERT INTO tracks (id, album_id, title, duration_seconds) VALUES (2, 1, 'Soul Love', 214);
INSERT INTO tracks (id, album_id, title, duration_seconds) VALUES (3, 1, 'Moonage Daydream', 280);
INSERT INTO tracks (id, album_id, title, duration_seconds) VALUES (4, 1, 'Starman', 250);
INSERT INTO tracks (id, album_id, title, duration_seconds) VALUES (5, 1, 'Lady Stardust', 200);
INSERT INTO tracks (id, album_id, title, duration_seconds) VALUES (6, 3, 'Station to Station', 614);
INSERT INTO tracks (id, album_id, title, duration_seconds) VALUES (7, 3, 'Golden Years', 240);
INSERT INTO tracks (id, album_id, title, duration_seconds) VALUES (8, 3, 'TVC 15', 333);
INSERT INTO tracks (id, album_id, title, duration_seconds) VALUES (9, 4, 'Speed of Life', 166);
INSERT INTO tracks (id, album_id, title, duration_seconds) VALUES (10, 4, 'Breaking Glass', 112);
INSERT INTO tracks (id, album_id, title, duration_seconds) VALUES (11, 4, 'Sound and Vision', 183);
INSERT INTO tracks (id, album_id, title, duration_seconds) VALUES (12, 5, 'Beauty and the Beast', 212);
INSERT INTO tracks (id, album_id, title, duration_seconds) VALUES (13, 5, 'Heroes', 370);
INSERT INTO tracks (id, album_id, title, duration_seconds) VALUES (14, 5, 'Sons of the Silent Age', 195);
INSERT INTO tracks (id, album_id, title, duration_seconds) VALUES (15, 6, 'Ashes to Ashes', 263);
INSERT INTO tracks (id, album_id, title, duration_seconds) VALUES (16, 6, 'Fashion', 288);

-- Query to find the heroes
SELECT t.title, a.title
FROM tracks t
         JOIN albums a ON t.album_id = a.id
WHERE t.title = 'Heroes';

-- Drop the mic
-- DROP TABLE albums; -- Commented out for safety