package io.github.hypocketapp.blocks

import androidx.annotation.ArrayRes
import androidx.annotation.StringRes

/**
 * The notes left in the world by the people the night creatures took: sheets
 * of paper and phones with their last messages. The game places them and says
 * which one was opened (BlocksStats.noteId); their words live here and in the
 * string resources. The ids and kinds must match note_kind[] in blocks.c.
 */
object StoryNotes {
    enum class Kind { PAPER, PHONE }

    data class Note(
        val id: Int,
        val kind: Kind,
        @param:StringRes val title: Int,
        /** A paper's text. */
        @param:StringRes val text: Int = 0,
        /** A phone's messages, each "sender|message"; the phone's owner has no name. */
        @param:ArrayRes val chat: Int = 0,
    )

    val ALL: List<Note> = listOf(
        Note(0, Kind.PAPER, R.string.story_note_0_title, text = R.string.story_note_0_text),
        Note(1, Kind.PAPER, R.string.story_note_1_title, text = R.string.story_note_1_text),
        Note(2, Kind.PHONE, R.string.story_note_2_title, chat = R.array.story_note_2_chat),
        Note(3, Kind.PAPER, R.string.story_note_3_title, text = R.string.story_note_3_text),
        Note(4, Kind.PHONE, R.string.story_note_4_title, chat = R.array.story_note_4_chat),
        Note(5, Kind.PAPER, R.string.story_note_5_title, text = R.string.story_note_5_text),
        Note(6, Kind.PHONE, R.string.story_note_6_title, chat = R.array.story_note_6_chat),
        Note(7, Kind.PAPER, R.string.story_note_7_title, text = R.string.story_note_7_text),
        Note(8, Kind.PAPER, R.string.story_note_8_title, text = R.string.story_note_8_text),
    )

    fun of(id: Int): Note? = ALL.getOrNull(id)

    /** The notes found, from the game's bit mask (bit n: note n). */
    fun found(mask: Int): List<Note> = ALL.filter { mask and (1 shl it.id) != 0 }

    /** One phone message: who sent it ("" for the phone's owner) and what it says. */
    fun message(item: String): Pair<String, String> {
        val bar = item.indexOf('|')
        return if (bar < 0) "" to item else item.substring(0, bar) to item.substring(bar + 1)
    }
}
