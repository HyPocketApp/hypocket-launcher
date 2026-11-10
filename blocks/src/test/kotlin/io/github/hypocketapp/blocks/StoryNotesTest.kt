package io.github.hypocketapp.blocks

import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import java.io.File

class StoryNotesTest {
    @Test
    fun idsAreTheirPlaceInTheList() {
        StoryNotes.ALL.forEachIndexed { i, note -> assertEquals(i, note.id) }
    }

    @Test
    fun kindsMatchTheGame() {
        // The game places the notes and draws papers and phones differently.
        val source = File("game/blocks.c").readText()
        val table = Regex("""note_kind\[NOTES\] = \{([^}]*)\}""").find(source)!!.groupValues[1]
        val kinds = Regex("NOTE_(PAPER|PHONE)").findAll(table).map { it.groupValues[1] }.toList()
        assertEquals(StoryNotes.ALL.map { it.kind.name }, kinds)
        val count = Regex("""#define NOTES (\d+)""").find(source)!!.groupValues[1].toInt()
        assertEquals(StoryNotes.ALL.size, count)
        // Found notes are kept from bit NOTE_SHIFT of a 32-bit field.
        val shift = Regex("""#define NOTE_SHIFT (\d+)""").find(source)!!.groupValues[1].toInt()
        assertTrue(shift + count <= 32)
    }

    @Test
    fun everyNoteHasItsWordsInBothLanguages() {
        for (dir in listOf("values", "values-pt-rBR")) {
            val strings = File("src/main/res/$dir/strings.xml").readText()
            for (note in StoryNotes.ALL) {
                assertTrue("$dir: title of ${note.id}", strings.contains("name=\"story_note_${note.id}_title\""))
                val body = if (note.kind == StoryNotes.Kind.PAPER) "string name=\"story_note_${note.id}_text\"" else "string-array name=\"story_note_${note.id}_chat\""
                assertTrue("$dir: words of ${note.id}", strings.contains(body))
            }
        }
    }

    @Test
    fun foundNotesComeFromTheMask() {
        assertEquals(listOf(0, 2, 8), StoryNotes.found(1 or 4 or 256).map { it.id })
        assertEquals(emptyList<Int>(), StoryNotes.found(0).map { it.id })
    }

    @Test
    fun phoneMessagesSplitSenderAndText() {
        assertEquals("" to "mom where are u", StoryNotes.message("|mom where are u"))
        assertEquals("Mom" to "Caio?", StoryNotes.message("Mom|Caio?"))
        assertEquals("" to "no bar", StoryNotes.message("no bar"))
    }
}
