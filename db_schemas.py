from datetime import datetime
from typing import Optional

from sqlmodel import Field, SQLModel


# User always has some session_id, but the TTL may indicate it has expired.
class User(SQLModel, table=True):
    user_id: str = Field(primary_key=True)
    username: str = Field(unique=True)
    password_hash: str
    session_id: str = Field(index=True)
    session_ttl: datetime


# is_complete with empty winner ID indicates the game ended in a draw.
# If one of the player_id slots is empty, and the is_public field is true,
# then this game is waiting for the 2nd player to join.
class Game(SQLModel, table=True):
    game_id: str = Field(primary_key=True)
    player_id_1: Optional[str] = Field(default=None, foreign_key="user.user_id")
    player_id_2: Optional[str] = Field(default=None, foreign_key="user.user_id")
    winner_id: Optional[str] = Field(default=None, foreign_key="user.user_id")
    is_complete: bool = Field(default=False)
    creation_time: datetime = Field(default_factory=datetime.utcnow)
    is_public: bool = Field(default=True)


class Move(SQLModel, table=True):
    game_id: str = Field(foreign_key="game.game_id", primary_key=True)
    move_index: int = Field(primary_key=True)
    a: bool  # row parity (0 or 1)
    r: int   # row
    c: int   # column
