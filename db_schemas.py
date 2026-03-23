from datetime import datetime
from typing import Optional

from sqlalchemy import Column, Index, LargeBinary
from sqlmodel import Field, SQLModel

ID_LEN_BYTES = 32
ID_LEN = (ID_LEN_BYTES * 3) // 2
UNAME_MAX = 64
CHATMESSAGE_MAX=256

# User always has some session_id, but the TTL may indicate it has expired.
class User(SQLModel, table=True):
    user_id: str = Field(max_length=ID_LEN, primary_key=True)
    username: str = Field(max_length=UNAME_MAX, unique=True)
    password_hash: bytes = Field(sa_column=Column(LargeBinary(60), nullable=False))
    session_id: str = Field(max_length=ID_LEN, index=True)
    session_ttl: datetime
    is_deleted: bool = Field(default=False)
    is_anon: bool = Field(default=False)
    bot_assist: bool = Field(default=False)


# is_complete with empty winner ID indicates the game ended in a draw.
# If one of the player_id slots is empty, and the is_public field is true,
# then this game is waiting for the 2nd player to join.
class Game(SQLModel, table=True):
    game_id: str = Field(max_length=ID_LEN, primary_key=True)
    player_id_1: Optional[str] = Field(max_length=ID_LEN, default=None, foreign_key="user.user_id")
    player_id_2: Optional[str] = Field(max_length=ID_LEN, default=None, foreign_key="user.user_id")
    winner_id: Optional[str] = Field(max_length=ID_LEN, default=None, foreign_key="user.user_id")
    is_complete: bool = Field(default=False)
    creation_time: datetime = Field(default_factory=datetime.utcnow)
    move_time: datetime = Field(default_factory=datetime.utcnow)
    is_public: bool = Field(default=True)
    last_req_p1: Optional[datetime] = Field(default=None)
    last_req_p2: Optional[datetime] = Field(default=None)
    rematch_offered: Optional[str] = Field(max_length=ID_LEN, default=None, foreign_key="user.user_id")
    rematch_id: Optional[str] = Field(max_length=ID_LEN, default=None, foreign_key="game.game_id")

    __table_args__ = (
        Index("ix_game_complete_last_req_p1", "is_complete", "last_req_p1"),
        Index("ix_game_complete_last_req_p2", "is_complete", "last_req_p2"),
        Index("ix_game_move_time", "move_time"),
        Index("ix_game_player_id_1", "player_id_1"),
        Index("ix_game_player_id_2", "player_id_2"),
    )


class Move(SQLModel, table=True):
    game_id: str = Field(max_length=ID_LEN, foreign_key="game.game_id", primary_key=True)
    move_index: int = Field(primary_key=True)
    a: bool  # row parity (0 or 1)
    r: int   # row
    c: int   # column


class BestMove(SQLModel, table=True):
    game_id: str = Field(max_length=ID_LEN, foreign_key="game.game_id", primary_key=True)
    move_index: int = Field(primary_key=True)
    evaluation: int = Field(default=0)
    a: int
    r: int
    c: int


class ChatMessage(SQLModel, table=True):
    game_id: str = Field(max_length=ID_LEN, foreign_key="game.game_id", primary_key=True)
    chat_id: int = Field(primary_key=True)
    message: str = Field(max_length=CHATMESSAGE_MAX)
    sender_id: str = Field(max_length=ID_LEN, foreign_key="user.user_id")
    sent_time: datetime = Field(default_factory=datetime.utcnow)

    __table_args__ = (
        Index("ix_game_sent_time", "sent_time"),
    )
