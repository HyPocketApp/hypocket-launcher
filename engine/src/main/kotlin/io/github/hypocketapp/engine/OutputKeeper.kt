package io.github.hypocketapp.engine

/**
 * Keeps the start and the end of a process's output within a size limit:
 * the first [headChars] characters, then the last lines up to [tailChars],
 * with a line saying how many lines were left out between them. The end is
 * where a game prints its summary and its errors on the way out. Safe to
 * use from the thread that reads the output and any other.
 */
internal class OutputKeeper(private val headChars: Int, private val tailChars: Int) {
    private val head = StringBuilder()
    private val tail = ArrayDeque<String>()
    private var tailLength = 0
    private var omitted = 0

    @Synchronized
    fun add(line: String) {
        if (tail.isEmpty() && head.length + line.length + 1 <= headChars) {
            head.append(line).append('\n')
            return
        }
        tail.addLast(line)
        tailLength += line.length + 1
        while (tailLength > tailChars && tail.isNotEmpty()) {
            tailLength -= tail.removeFirst().length + 1
            omitted++
        }
    }

    @Synchronized
    override fun toString(): String = buildString {
        append(head)
        if (omitted > 0) append("... ").append(omitted).append(" lines left out ...\n")
        tail.forEach { append(it).append('\n') }
    }
}
