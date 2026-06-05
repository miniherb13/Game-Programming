"""Keyword replies for LCD (English input for VNC)."""

KEYWORD_REPLIES = {
    "hello": ("Hi!", "Nice to meet"),
    "hi": ("Hi!", "Nice to meet"),
    "mood": ("I feel good!", "How about you?"),
    "feel": ("I feel good!", "How about you?"),
    "name": ("I am Bot", "Your friend"),
    "who": ("I am Bot", "Your friend"),
    "what": ("Watching you!", "Say hello~"),
    "doing": ("Watching you!", "Say hello~"),
    "thanks": ("You're welcome", "Anytime!"),
    "thank": ("You're welcome", "Anytime!"),
    "bye": ("See you!", "Take care~"),
    "night": ("Good night!", "Sweet dreams"),
}


def _split_two_lines(text: str) -> tuple[str, str]:
    text = text.strip()
    if len(text) <= 16:
        return text, ""
    return text[:16], text[16:32]


def get_reply(user_text: str) -> tuple[str, str]:
    """User text -> (line1, line2), max 16 chars each."""
    key = user_text.strip().lower()
    for keyword, reply in KEYWORD_REPLIES.items():
        if keyword in key:
            return reply
    return _split_two_lines("I heard you! Tell me more.")


if __name__ == "__main__":
    while True:
        try:
            q = input("Question: ").strip()
        except (EOFError, KeyboardInterrupt):
            break
        if not q:
            continue
        print(get_reply(q))
