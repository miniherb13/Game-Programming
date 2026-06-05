"""키워드·LLM 답변 — A 담당. API 없으면 KEYWORD_REPLIES만 사용."""

KEYWORD_REPLIES = {
    "안녕": ("Hi!", "Nice to meet"),
    "hello": ("Hi!", "Nice to meet"),
    "기분": ("I feel good!", "How about you?"),
    "mood": ("I feel good!", "How about you?"),
    "이름": ("I am Bot", "Your friend"),
    "name": ("I am Bot", "Your friend"),
    "뭐해": ("Watching you!", "Say hello~"),
    "what": ("Watching you!", "Say hello~"),
    "고마워": ("You're welcome", "Anytime!"),
    "thanks": ("You're welcome", "Anytime!"),
    "잘자": ("Good night!", "Sweet dreams"),
    "bye": ("See you!", "Take care~"),
}


def _split_two_lines(text: str) -> tuple[str, str]:
    text = text.strip()
    if len(text) <= 16:
        return text, ""
    return text[:16], text[16:32]


def get_reply(user_text: str) -> tuple[str, str]:
    """사용자 입력 → LCD용 (line1, line2), 각 16자 이내."""
    key = user_text.strip().lower()
    for keyword, reply in KEYWORD_REPLIES.items():
        if keyword in key:
            return reply
    return _split_two_lines("I heard you! Tell me more.")


if __name__ == "__main__":
    while True:
        try:
            q = input("질문: ").strip()
        except (EOFError, KeyboardInterrupt):
            break
        if not q:
            continue
        print(get_reply(q))
