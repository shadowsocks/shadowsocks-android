package scala.collection.mutable

import scala.collection.SortedSetLike
import scala.collection.generic.MutableSortedSetFactory
import scala.collection.immutable.RedBlackTree.Tree
import scala.collection.immutable.{RedBlackTree => RB}
import scala.runtime.ObjectRef

/**
  * @define Coll `mutable.SortedList`
  * @define coll mutable sorted list
  * @factoryInfo
  *   Companion object of SortedList providing factory related utilities.
  * @author Mygod
  */
//noinspection ReferenceMustBePrefixed,ScalaDocUnknownTag
object SortedList extends MutableSortedSetFactory[SortedList] {
  /**
    *  The empty list of this type
    */
  def empty[A](implicit ordering: Ordering[A]) = new SortedList[A]()
}

/**
  * A mutable SortedList heavily based on SortedSet which "is not designed to enable meaningful subclassing".
  *
  * Based on: https://github.com/scala/scala/blob/3cc99d7/src/library/scala/collection/mutable/TreeSet.scala
  *
  * @author Mygod
  */
//noinspection ReferenceMustBePrefixed
class SortedList[A] private(treeRef: ObjectRef[RB.Tree[A, Null]], from: Option[A], until: Option[A])
                           (implicit val ordering: Ordering[A])
  extends SortedSet[A] with SetLike[A, SortedList[A]] with SortedSetLike[A, SortedList[A]] with Set[A] with Serializable {
  def insert(elem: A): Boolean = {
    val old = size
    this += elem
    old < size
  }

  // AbstractSeq
  def apply(i: Int): A = try RB.nth(treeRef.elem, i).key catch {
    // out of bounds will result in accessing null nodes
    case e: NullPointerException => throw new IndexOutOfBoundsException().initCause(e)
  }
  def length: Int = size

  // Buffer
  def +=:(elem: A): SortedList.this.type = +=(elem)  // prepending is the same as the list is sorted
  def remove(n: Int): A = {
    val result = apply(n)
    remove(result)
    result
  }
  def update(idx: Int, elem: A) {
    val x = apply(idx)
    if (!ordering.equiv(x, elem)) {
      remove(x)
      this.add(elem)
    }
  }

  // Optimized methods
  override def clear(): Unit = treeRef.elem = null
  def indexOf[B >: A](elem: B, from: Int = 0): Int = lookupIndex(treeRef.elem, elem.asInstanceOf[A]) match {
    case result if result < from => -1
    case result => result
  }
  override def last: A = RB.greatest(treeRef.elem).key

  // Tree helper methods, TODO: expand to non-recursive form?
  private def lookupIndex(tree: Tree[A, Null], x: A): Int =
    if (tree eq null) -1 else ordering.compare(x, tree.key) match {
      case 0 => RB.count(tree.left)
      case cmp if cmp < 0 => lookupIndex(tree.left, x)
      case _ => lookupIndex(tree.right, x) match {
        case i if i < 0 => i
        case i => RB.count(tree.left) + 1 + i
      }
    }

  // The following is copied from TreeSet

  if (ordering eq null)
    throw new NullPointerException("ordering must not be null")

  def this()(implicit ordering: Ordering[A]) = this(new ObjectRef(null), None, None)

  override def size: Int = RB.countInRange(treeRef.elem, from, until)

  override def stringPrefix = "SortedList"

  override def empty: SortedList[A] = SortedList.empty

  private def pickBound(comparison: (A, A) => A, oldBound: Option[A], newBound: Option[A]) = (newBound, oldBound) match {
    case (Some(newB), Some(oldB)) => Some(comparison(newB, oldB))
    case (None, _) => oldBound
    case _ => newBound
  }

  override def rangeImpl(fromArg: Option[A], untilArg: Option[A]): SortedList[A] = {
    val newFrom = pickBound(ordering.max, fromArg, from)
    val newUntil = pickBound(ordering.min, untilArg, until)

    new SortedList(treeRef, newFrom, newUntil)
  }

  override def -=(elem: A): this.type = {
    treeRef.elem = RB.delete(treeRef.elem, elem)
    this
  }

  override def +=(elem: A): this.type = {
    treeRef.elem = RB.update(treeRef.elem, elem, null, overwrite = false)
    this
  }

  /**
    * Thanks to the immutable nature of the
    * underlying Tree, we can share it with
    * the clone. So clone complexity in time is O(1).
    *
    */
  override def clone(): SortedList[A] =
    new SortedList[A](new ObjectRef(treeRef.elem), from, until)

  private val notProjection = !(from.isDefined || until.isDefined)

  override def contains(elem: A): Boolean = {
    def leftAcceptable: Boolean = from match {
      case Some(lb) => ordering.gteq(elem, lb)
      case _ => true
    }

    def rightAcceptable: Boolean = until match {
      case Some(ub) => ordering.lt(elem, ub)
      case _ => true
    }

    (notProjection || (leftAcceptable && rightAcceptable)) &&
      RB.contains(treeRef.elem, elem)
  }

  override def iterator: Iterator[A] = iteratorFrom(None)

  override def keysIteratorFrom(start: A): Iterator[A] = iteratorFrom(Some(start))

  private def iteratorFrom(start: Option[A]) = {
    val it = RB.keysIterator(treeRef.elem, pickBound(ordering.max, from, start))
    until match {
      case None => it
      case Some(ub) => it takeWhile (k => ordering.lt(k, ub))
    }
  }
}
