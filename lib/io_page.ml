(*
 * Copyright (c) 2011-2012 Anil Madhavapeddy <anil@recoil.org>
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

type t = (char, Bigarray.int8_unsigned_elt, Bigarray.c_layout) Bigarray.Array1.t

external alloc_pages: int -> t = "caml_alloc_pages"

let page_size = 1 lsl 12

let length t = Bigarray.Array1.dim t

let cache = Stack.create ()

let get_unsafe n =
  try alloc_pages n with _ ->
    Gc.compact ();
    try alloc_pages n with _ -> raise Out_of_memory

let get = function
  | n when n < 1 ->
    raise (Invalid_argument "The number of page should be greater or equal to 1")
  | 1 -> if Stack.is_empty cache then get_unsafe 1 else Stack.pop cache
  | n -> get_unsafe n

let recycle frame =
  if Bigarray.Array1.dim frame <> page_size
  then raise (Invalid_argument "block size is not 4096 and therefore cannot be recycled")
  else Stack.push frame cache

let get_order order = get (1 lsl order)

let to_pages t =
  assert ((length t) mod page_size == 0);
  let rec loop off acc =
    if (off < (length t))
      then loop (off + page_size) (Bigarray.Array1.sub t off page_size :: acc)
      else acc
  in
  List.rev (loop 0 [])

let pages n =
  let rec inner acc n =
    if n > 0 then inner ((get 1)::acc) (n - 1) else acc
  in
  inner [] n

let pages_order order = pages (1 lsl order)

let to_cstruct t =
  Cstruct.of_bigarray t

let string_blit src t =
  for i = 0 to ((String.length src) - 1) do
    t.{i} <- src.[i]
  done

let to_string t =
  let result = String.create (length t) in
  for i = 0 to ((length t) - 1) do
    result.[i] <- t.{i}
  done;
  result

let blit src dest = Bigarray.Array1.blit src dest

let string_blit src srcoff dst dstoff len =
  for i = srcoff to srcoff + len - 1 do
    dst.{i + dstoff} <- src.[i]
  done
