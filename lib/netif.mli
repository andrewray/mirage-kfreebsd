(*
 * Copyright (c) 2011 Anil Madhavapeddy <anil@recoil.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *)

(** kFreeBSD network interface for Ethernet I/O *)

(** Type repesenting a network interface, containing low level data
    structures to read and write from it. *)
type t

(** Textual id which is unique per interface, e.g. "em0". *)
type id

val id_of_string: string -> id
val string_of_id: id -> string

(** Accessors for the t type *)

val get_writebuf : t -> Cstruct.t Lwt.t
(** [id if] is the id of interface [if]. *)
val id           : t -> id
(** [mac if] is the MAC address of [if]. *)
val mac          : t -> Macaddr.t

(** [create ()] is a thread that returns a list of initialized network
    interfaces. *)
val create : unit -> (t list) Lwt.t

(** [write if buf] outputs [buf] to interface [if]. *)
val write : t -> Cstruct.t -> unit Lwt.t

(** [writev if bufs] output a list of buffers to interface [if] as a
    single packet. *)
val writev : t -> Cstruct.t list -> unit Lwt.t

(** [listen if cb] is a thread that listens endlesses on [if], and
    invoke the callback function as frames are received. *)
val listen : t -> (Cstruct.t -> unit Lwt.t) -> unit Lwt.t
