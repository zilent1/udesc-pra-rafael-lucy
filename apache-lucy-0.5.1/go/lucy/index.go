/* Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package lucy

/*
#include "Lucy/Index/Indexer.h"
#include "Lucy/Index/IndexReader.h"
#include "Lucy/Index/DataReader.h"
#include "Lucy/Index/DataWriter.h"
#include "Lucy/Index/SegWriter.h"
#include "Lucy/Index/DeletionsWriter.h"
#include "Lucy/Index/DocReader.h"
#include "Lucy/Index/LexiconReader.h"
#include "Lucy/Index/PostingListReader.h"
#include "Lucy/Index/HighlightReader.h"
#include "Lucy/Index/SortReader.h"
#include "Lucy/Index/IndexManager.h"
#include "Lucy/Index/BackgroundMerger.h"
#include "Lucy/Index/TermVector.h"
#include "Lucy/Index/Segment.h"
#include "Lucy/Index/Snapshot.h"
#include "Lucy/Index/SortCache.h"
#include "Lucy/Document/Doc.h"
#include "Lucy/Plan/Schema.h"
#include "Clownfish/Hash.h"
#include "Clownfish/String.h"
#include "Clownfish/Vector.h"
#include "Clownfish/Err.h"
*/
import "C"
import "fmt"
import "reflect"
import "strings"
import "unsafe"
import "git-wip-us.apache.org/repos/asf/lucy-clownfish.git/runtime/go/clownfish"

type IndexerIMP struct {
	clownfish.ObjIMP
	fieldNames map[string]string
}

type OpenIndexerArgs struct {
	Schema   Schema
	Index    interface{}
	Manager  IndexManager
	Create   bool
	Truncate bool
}

func OpenIndexer(args *OpenIndexerArgs) (obj Indexer, err error) {
	schema := (*C.lucy_Schema)(clownfish.UnwrapNullable(args.Schema))
	manager := (*C.lucy_IndexManager)(clownfish.UnwrapNullable(args.Manager))
	index := (*C.cfish_Obj)(clownfish.GoToClownfish(args.Index, unsafe.Pointer(C.CFISH_OBJ), false))
	defer C.cfish_decref(unsafe.Pointer(index))
	var flags int32
	if args.Create {
		flags = flags | int32(C.lucy_Indexer_CREATE)
	}
	if args.Truncate {
		flags = flags | int32(C.lucy_Indexer_TRUNCATE)
	}
	err = clownfish.TrapErr(func() {
		cfObj := C.lucy_Indexer_new(schema, index, manager, C.int32_t(flags))
		obj = WRAPIndexer(unsafe.Pointer(cfObj))
	})
	return obj, err
}

func (obj *IndexerIMP) Close() error {
	// TODO: implement Close in core Lucy rather than bindings.
	return nil // TODO catch errors
}

func (obj *IndexerIMP) addDocObj(doc Doc, boost float32) error {
	self := (*C.lucy_Indexer)(clownfish.Unwrap(obj, "obj"))
	d := (*C.lucy_Doc)(clownfish.Unwrap(doc, "doc"))
	return clownfish.TrapErr(func() {
		C.LUCY_Indexer_Add_Doc(self, d, C.float(boost))
	})
}

func (obj *IndexerIMP) addMapAsDoc(doc map[string]interface{}, boost float32) error {
	self := (*C.lucy_Indexer)(clownfish.Unwrap(obj, "obj"))
	d := C.LUCY_Indexer_Get_Stock_Doc(self)
	docFields := fetchDocFields(d)
	for field := range docFields {
		delete(docFields, field)
	}
	for key, value := range doc {
		field, err := obj.findRealField(key)
		if err != nil {
			return err
		}
		docFields[field] = value
	}
	return clownfish.TrapErr(func() {
		C.LUCY_Indexer_Add_Doc(self, d, C.float(boost))
	})
}

func (obj *IndexerIMP) addStructAsDoc(doc interface{}, boost float32) error {
	self := (*C.lucy_Indexer)(clownfish.Unwrap(obj, "obj"))
	d := C.LUCY_Indexer_Get_Stock_Doc(self)
	docFields := fetchDocFields(d)
	for field := range docFields {
		delete(docFields, field)
	}

	// Get reflection value and type for the supplied struct.
	var docValue reflect.Value
	var success bool
	if reflect.ValueOf(doc).Kind() == reflect.Ptr {
		temp := reflect.ValueOf(doc).Elem()
		if temp.Kind() == reflect.Struct {
			docValue = temp
			success = true
		}
	}
	if !success {
		mess := fmt.Sprintf("Unexpected type for doc: %t", doc)
		return clownfish.NewErr(mess)
	}

	// Copy field values into stockDoc.
	docType := docValue.Type()
	for i := 0; i < docValue.NumField(); i++ {
		field := docType.Field(i).Name
		value := docValue.Field(i).String()
		realField, err := obj.findRealField(field)
		if err != nil {
			return err
		}
		docFields[realField] = value
	}

	return clownfish.TrapErr(func() {
		C.LUCY_Indexer_Add_Doc(self, d, C.float(boost))
	})
}

func (obj *IndexerIMP) AddDoc(doc interface{}) error {
	// TODO create an additional method AddDocWithBoost which allows the
	// client to supply `boost`.
	boost := float32(1.0)

	if suppliedDoc, ok := doc.(Doc); ok {
		return obj.addDocObj(suppliedDoc, boost)
	} else if m, ok := doc.(map[string]interface{}); ok {
		return obj.addMapAsDoc(m, boost)
	} else {
		return obj.addStructAsDoc(doc, boost)
	}
}

func (obj *IndexerIMP) findRealField(name string) (string, error) {
	self := ((*C.lucy_Indexer)(unsafe.Pointer(obj.TOPTR())))
	if obj.fieldNames == nil {
		obj.fieldNames = make(map[string]string)
	}
	if field, ok := obj.fieldNames[name]; ok {
		return field, nil
	} else {
		schema := C.LUCY_Indexer_Get_Schema(self)
		fieldList := C.LUCY_Schema_All_Fields(schema)
		defer C.cfish_dec_refcount(unsafe.Pointer(fieldList))
		for i := 0; i < int(C.CFISH_Vec_Get_Size(fieldList)); i++ {
			cfString := unsafe.Pointer(C.CFISH_Vec_Fetch(fieldList, C.size_t(i)))
			field := clownfish.CFStringToGo(cfString)
			if strings.EqualFold(name, field) {
				obj.fieldNames[name] = field
				return field, nil
			}
		}
	}
	return "", clownfish.NewErr(fmt.Sprintf("Unknown field: '%v'", name))
}

func (obj *IndexerIMP) AddIndex(index interface{}) error {
	return clownfish.TrapErr(func() {
		self := (*C.lucy_Indexer)(clownfish.Unwrap(obj, "obj"))
		indexC := (*C.cfish_Obj)(clownfish.GoToClownfish(index, unsafe.Pointer(C.CFISH_OBJ), false))
		defer C.cfish_decref(unsafe.Pointer(indexC))
		C.LUCY_Indexer_Add_Index(self, indexC)
	})
}

func (obj *IndexerIMP) DeleteByTerm(field string, term interface{}) error {
	return clownfish.TrapErr(func() {
		self := (*C.lucy_Indexer)(clownfish.Unwrap(obj, "obj"))
		fieldC := (*C.cfish_String)(clownfish.GoToClownfish(field, unsafe.Pointer(C.CFISH_STRING), false))
		defer C.cfish_decref(unsafe.Pointer(fieldC))
		termC := (*C.cfish_Obj)(clownfish.GoToClownfish(term, unsafe.Pointer(C.CFISH_OBJ), false))
		defer C.cfish_decref(unsafe.Pointer(termC))
		C.LUCY_Indexer_Delete_By_Term(self, fieldC, termC)
	})
}

func (obj *IndexerIMP) DeleteByQuery(query Query) error {
	return clownfish.TrapErr(func() {
		self := (*C.lucy_Indexer)(clownfish.Unwrap(obj, "obj"))
		queryC := (*C.lucy_Query)(clownfish.Unwrap(query, "query"))
		C.LUCY_Indexer_Delete_By_Query(self, queryC)
	})
}

func (obj *IndexerIMP) DeleteByDocID(docID int32) error {
	return clownfish.TrapErr(func() {
		self := (*C.lucy_Indexer)(clownfish.Unwrap(obj, "obj"))
		C.LUCY_Indexer_Delete_By_Doc_ID(self, C.int32_t(docID))
	})
}

func (obj *IndexerIMP) PrepareCommit() error {
	self := ((*C.lucy_Indexer)(unsafe.Pointer(obj.TOPTR())))
	return clownfish.TrapErr(func() {
		C.LUCY_Indexer_Prepare_Commit(self)
	})
}

func (obj *IndexerIMP) Commit() error {
	self := ((*C.lucy_Indexer)(unsafe.Pointer(obj.TOPTR())))
	return clownfish.TrapErr(func() {
		C.LUCY_Indexer_Commit(self)
	})
}

func (d *DataWriterIMP) addInvertedDoc(inverter Inverter, docId int32) error {
	return clownfish.TrapErr(func() {
		self := (*C.lucy_DataWriter)(clownfish.Unwrap(d, "d"))
		inverterCF := (*C.lucy_Inverter)(clownfish.Unwrap(inverter, "inverter"))
		C.LUCY_DataWriter_Add_Inverted_Doc(self, inverterCF, C.int32_t(docId))
	})
}

func (d *DataWriterIMP) AddSegment(reader SegReader, docMap []int32) error {
	return clownfish.TrapErr(func() {
		self := (*C.lucy_DataWriter)(clownfish.Unwrap(d, "d"))
		readerCF := (*C.lucy_SegReader)(clownfish.Unwrap(reader, "reader"))
		docMapConv := NewI32Array(docMap)
		docMapCF := (*C.lucy_I32Array)(clownfish.UnwrapNullable(docMapConv))
		C.LUCY_DataWriter_Add_Segment(self, readerCF, docMapCF)
	})
}

func (d *DataWriterIMP) DeleteSegment(reader SegReader) error {
	return clownfish.TrapErr(func() {
		self := (*C.lucy_DataWriter)(clownfish.Unwrap(d, "d"))
		readerCF := (*C.lucy_SegReader)(clownfish.Unwrap(reader, "reader"))
		C.LUCY_DataWriter_Delete_Segment(self, readerCF)
	})
}

func (d *DataWriterIMP) MergeSegment(reader SegReader, docMap []int32) error {
	return clownfish.TrapErr(func() {
		self := (*C.lucy_DataWriter)(clownfish.Unwrap(d, "d"))
		readerCF := (*C.lucy_SegReader)(clownfish.Unwrap(reader, "reader"))
		docMapConv := NewI32Array(docMap)
		docMapCF := (*C.lucy_I32Array)(clownfish.UnwrapNullable(docMapConv))
		C.LUCY_DataWriter_Merge_Segment(self, readerCF, docMapCF)
	})
}

func (d *DataWriterIMP) Finish() error {
	return clownfish.TrapErr(func() {
		self := (*C.lucy_DataWriter)(clownfish.Unwrap(d, "d"))
		C.LUCY_DataWriter_Finish(self)
	})
}

func (s *SegWriterIMP) PrepSegDir() error {
	return clownfish.TrapErr(func() {
		self := (*C.lucy_SegWriter)(clownfish.Unwrap(s, "s"))
		C.LUCY_SegWriter_Prep_Seg_Dir(self)
	})
}

func (s *SegWriterIMP) AddDoc(doc Doc, boost float32) error {
	return clownfish.TrapErr(func() {
		self := (*C.lucy_SegWriter)(clownfish.Unwrap(s, "s"))
		docCF := (*C.lucy_Doc)(clownfish.Unwrap(doc, "doc"))
		C.LUCY_SegWriter_Add_Doc(self, docCF, C.float(boost))
	})
}

func (d *DeletionsWriterIMP) DeleteByTerm(field string, term interface{}) error {
	return clownfish.TrapErr(func() {
		self := (*C.lucy_DeletionsWriter)(clownfish.Unwrap(d, "d"))
		fieldCF := (*C.cfish_String)(clownfish.GoToClownfish(field, unsafe.Pointer(C.CFISH_STRING), false))
		defer C.cfish_decref(unsafe.Pointer(fieldCF))
		termCF := (*C.cfish_Obj)(clownfish.GoToClownfish(term, unsafe.Pointer(C.CFISH_OBJ), false))
		defer C.cfish_decref(unsafe.Pointer(termCF))
		C.LUCY_DelWriter_Delete_By_Term(self, fieldCF, termCF)
	})
}

func (d *DeletionsWriterIMP) DeleteByQuery(query Query) error {
	return clownfish.TrapErr(func() {
		self := (*C.lucy_DeletionsWriter)(clownfish.Unwrap(d, "d"))
		queryCF := (*C.lucy_Query)(clownfish.Unwrap(query, "query"))
		C.LUCY_DelWriter_Delete_By_Query(self, queryCF)
	})
}

func (d *DeletionsWriterIMP) deleteByDocID(docId int32) error {
	return clownfish.TrapErr(func() {
		self := (*C.lucy_DeletionsWriter)(clownfish.Unwrap(d, "d"))
		C.LUCY_DelWriter_Delete_By_Doc_ID(self, C.int32_t(docId))
	})
}

func (d *DeletionsWriterIMP) generateDocMap(deletions Matcher, docMax int32, offset int32) (retval []int32, err error) {
	err = clownfish.TrapErr(func() {
		self := (*C.lucy_DeletionsWriter)(clownfish.Unwrap(d, "d"))
		deletionsCF := (*C.lucy_Matcher)(clownfish.Unwrap(deletions, "deletions"))
		retvalCF := C.LUCY_DelWriter_Generate_Doc_Map(self, deletionsCF, C.int32_t(docMax), C.int32_t(offset))
		defer C.cfish_decref(unsafe.Pointer(retvalCF))
		retval = i32ArrayToSlice(retvalCF)
	})
	return retval, err
}

func (d *DeletionsWriterIMP) segDeletions(segReader SegReader) (retval Matcher, err error) {
	err = clownfish.TrapErr(func() {
		self := (*C.lucy_DeletionsWriter)(clownfish.Unwrap(d, "d"))
		segReaderCF := (*C.lucy_SegReader)(clownfish.Unwrap(segReader, "segReader"))
		retvalCF := C.LUCY_DelWriter_Seg_Deletions(self, segReaderCF)
		if retvalCF != nil {
			retval = clownfish.WRAPAny(unsafe.Pointer(retvalCF)).(Matcher)
		}
	})
	return retval, err
}

func OpenBackgroundMerger(index interface{}, manager IndexManager) (bgm BackgroundMerger, err error) {
	err = clownfish.TrapErr(func() {
		indexC := (*C.cfish_Obj)(clownfish.GoToClownfish(index, unsafe.Pointer(C.CFISH_OBJ), false))
		defer C.cfish_decref(unsafe.Pointer(indexC))
		managerC := (*C.lucy_IndexManager)(clownfish.UnwrapNullable(manager))
		cfObj := C.lucy_BGMerger_new(indexC, managerC)
		bgm = WRAPBackgroundMerger(unsafe.Pointer(cfObj))
	})
	return bgm, err
}

func (bgm *BackgroundMergerIMP) PrepareCommit() error {
	return clownfish.TrapErr(func() {
		self := (*C.lucy_BackgroundMerger)(clownfish.Unwrap(bgm, "bgm"))
		C.LUCY_BGMerger_Prepare_Commit(self)
	})
}

func (bgm *BackgroundMergerIMP) Commit() error {
	return clownfish.TrapErr(func() {
		self := (*C.lucy_BackgroundMerger)(clownfish.Unwrap(bgm, "bgm"))
		C.LUCY_BGMerger_Commit(self)
	})
}

func (im *IndexManagerIMP) WriteMergeData(cutoff int64) error {
	return clownfish.TrapErr(func() {
		self := (*C.lucy_IndexManager)(clownfish.Unwrap(im, "im"))
		C.LUCY_IxManager_Write_Merge_Data(self, C.int64_t(cutoff))
	})
}

func (im *IndexManagerIMP) ReadMergeData() (retval map[string]interface{}, err error) {
	err = clownfish.TrapErr(func() {
		self := (*C.lucy_IndexManager)(clownfish.Unwrap(im, "im"))
		retvalC := C.LUCY_IxManager_Read_Merge_Data(self)
		if retvalC != nil {
			defer C.cfish_decref(unsafe.Pointer(retvalC))
			retval = clownfish.ToGo(unsafe.Pointer(retvalC)).(map[string]interface{})
		}
	})
	return retval, err
}

func (im *IndexManagerIMP) RemoveMergeData() error {
	return clownfish.TrapErr(func() {
		self := (*C.lucy_IndexManager)(clownfish.Unwrap(im, "im"))
		C.LUCY_IxManager_Remove_Merge_Data(self)
	})
}

func (im *IndexManagerIMP) MakeSnapshotFilename() (retval string, err error) {
	err = clownfish.TrapErr(func() {
		self := (*C.lucy_IndexManager)(clownfish.Unwrap(im, "im"))
		retvalC := C.LUCY_IxManager_Make_Snapshot_Filename(self)
		if retvalC != nil {
			defer C.cfish_decref(unsafe.Pointer(retvalC))
			retval = clownfish.ToGo(unsafe.Pointer(retvalC)).(string)
		}
	})
	return retval, err
}

func (im *IndexManagerIMP) Recycle(reader PolyReader, delWriter DeletionsWriter,
	cutoff int64, optimize bool) (retval []SegReader, err error) {
	err = clownfish.TrapErr(func() {
		self := (*C.lucy_IndexManager)(clownfish.Unwrap(im, "im"))
		readerC := (*C.lucy_PolyReader)(clownfish.Unwrap(reader, "reader"))
		delWriterC := (*C.lucy_DeletionsWriter)(clownfish.Unwrap(delWriter, "delWriter"))
		vec := C.LUCY_IxManager_Recycle(self, readerC, delWriterC,
			C.int64_t(cutoff), C.bool(optimize))
		if vec != nil {
			defer C.cfish_decref(unsafe.Pointer(vec))
			size := int(C.CFISH_Vec_Get_Size(vec))
			retval = make([]SegReader, size)
			for i := 0; i < size; i++ {
				elem := C.CFISH_Vec_Fetch(vec, C.size_t(i))
				retval[i] = WRAPSegReader(unsafe.Pointer(C.cfish_incref(unsafe.Pointer(elem))))
			}
		}
	})
	return retval, err
}

func NewTermVector(field, text string, positions, startOffsets, endOffsets []int32) TermVector {
	fieldC := (*C.cfish_String)(clownfish.GoToClownfish(field, unsafe.Pointer(C.CFISH_STRING), false))
	textC := (*C.cfish_String)(clownfish.GoToClownfish(text, unsafe.Pointer(C.CFISH_STRING), false))
	defer C.cfish_decref(unsafe.Pointer(fieldC))
	defer C.cfish_decref(unsafe.Pointer(textC))
	posits := NewI32Array(positions)
	starts := NewI32Array(startOffsets)
	ends := NewI32Array(endOffsets)
	positsC := (*C.lucy_I32Array)(clownfish.Unwrap(posits, "posits"))
	startsC := (*C.lucy_I32Array)(clownfish.Unwrap(starts, "starts"))
	endsC := (*C.lucy_I32Array)(clownfish.Unwrap(ends, "ends"))
	retvalC := C.lucy_TV_new(fieldC, textC, positsC, startsC, endsC)
	return WRAPTermVector(unsafe.Pointer(retvalC))
}

func (tv *TermVectorIMP) GetPositions() []int32 {
	self := (*C.lucy_TermVector)(clownfish.Unwrap(tv, "tv"))
	return i32ArrayToSlice(C.LUCY_TV_Get_Positions(self))
}

func (tv *TermVectorIMP) GetStartOffsets() []int32 {
	self := (*C.lucy_TermVector)(clownfish.Unwrap(tv, "tv"))
	return i32ArrayToSlice(C.LUCY_TV_Get_Start_Offsets(self))
}

func (tv *TermVectorIMP) GetEndOffsets() []int32 {
	self := (*C.lucy_TermVector)(clownfish.Unwrap(tv, "tv"))
	return i32ArrayToSlice(C.LUCY_TV_Get_End_Offsets(self))
}

func (s *SnapshotIMP) List() []string {
	self := (*C.lucy_Snapshot)(clownfish.Unwrap(s, "s"))
	retvalC := C.LUCY_Snapshot_List(self)
	defer C.cfish_decref(unsafe.Pointer(retvalC))
	return vecToStringSlice(retvalC)
}

func (s *SnapshotIMP) ReadFile(folder Folder, path string) (Snapshot, error) {
	err := clownfish.TrapErr(func() {
		self := (*C.lucy_Snapshot)(clownfish.Unwrap(s, "s"))
		folderC := (*C.lucy_Folder)(clownfish.Unwrap(folder, "folder"))
		pathC := (*C.cfish_String)(clownfish.GoToClownfish(path, unsafe.Pointer(C.CFISH_STRING), false))
		defer C.cfish_decref(unsafe.Pointer(pathC))
		C.LUCY_Snapshot_Read_File(self, folderC, pathC)
	})
	return s, err
}

func (s *SnapshotIMP) WriteFile(folder Folder, path string) error {
	return clownfish.TrapErr(func() {
		self := (*C.lucy_Snapshot)(clownfish.Unwrap(s, "s"))
		folderC := (*C.lucy_Folder)(clownfish.Unwrap(folder, "folder"))
		pathC := (*C.cfish_String)(clownfish.GoToClownfish(path, unsafe.Pointer(C.CFISH_STRING), false))
		defer C.cfish_decref(unsafe.Pointer(pathC))
		C.LUCY_Snapshot_Write_File(self, folderC, pathC)
	})
}

func (s *SegmentIMP) WriteFile(folder Folder) error {
	return clownfish.TrapErr(func() {
		self := (*C.lucy_Segment)(clownfish.Unwrap(s, "s"))
		folderC := (*C.lucy_Folder)(clownfish.Unwrap(folder, "folder"))
		C.LUCY_Seg_Write_File(self, folderC)
	})
}

func (s *SegmentIMP) ReadFile(folder Folder) error {
	return clownfish.TrapErr(func() {
		self := (*C.lucy_Segment)(clownfish.Unwrap(s, "s"))
		folderC := (*C.lucy_Folder)(clownfish.Unwrap(folder, "folder"))
		C.LUCY_Seg_Read_File(self, folderC)
	})
}

func (s *SortCacheIMP) Value(ord int32) (retval interface{}, err error) {
	err = clownfish.TrapErr(func() {
		self := (*C.lucy_SortCache)(clownfish.Unwrap(s, "s"))
		retvalCF := C.LUCY_SortCache_Value(self, C.int32_t(ord))
		defer C.cfish_decref(unsafe.Pointer(retvalCF))
		retval = clownfish.ToGo(unsafe.Pointer(retvalCF))
	})
	return retval, err
}

func (s *SortCacheIMP) Ordinal(docId int32) (retval int32, err error) {
	err = clownfish.TrapErr(func() {
		self := (*C.lucy_SortCache)(clownfish.Unwrap(s, "s"))
		retvalCF := C.LUCY_SortCache_Ordinal(self, C.int32_t(docId))
		retval = int32(retvalCF)
	})
	return retval, err
}

func (s *SortCacheIMP) Find(term interface{}) (retval int32, err error) {
	err = clownfish.TrapErr(func() {
		self := (*C.lucy_SortCache)(clownfish.Unwrap(s, "s"))
		termCF := (*C.cfish_Obj)(clownfish.GoToClownfish(term, unsafe.Pointer(C.CFISH_OBJ), true))
		defer C.cfish_decref(unsafe.Pointer(termCF))
		retvalCF := C.LUCY_SortCache_Find(self, termCF)
		retval = int32(retvalCF)
	})
	return retval, err
}

func (d *DataReaderIMP) Aggregator(readers []DataReader, offsets []int32) (retval DataReader, err error) {
	err = clownfish.TrapErr(func() {
		self := (*C.lucy_DataReader)(clownfish.Unwrap(d, "d"))
		size := len(readers)
		readersC := C.cfish_Vec_new(C.size_t(size))
		defer C.cfish_decref(unsafe.Pointer(readersC))
		for i := 0; i < size; i++ {
			elemC := clownfish.Unwrap(readers[i], "readers[i]")
			C.CFISH_Vec_Push(readersC, C.cfish_incref(elemC))
		}
		offs := NewI32Array(offsets)
		offsetsC := (*C.lucy_I32Array)(clownfish.Unwrap(offs, "offs"))
		retvalCF := C.LUCY_DataReader_Aggregator(self, readersC, offsetsC)
		defer C.cfish_decref(unsafe.Pointer(retvalCF))
		if retvalCF != nil {
			retval = clownfish.ToGo(unsafe.Pointer(retvalCF)).(DataReader)
		}
	})
	return retval, err
}

func (d *DataReaderIMP) GetSegments() []Segment {
	self := (*C.lucy_DataReader)(clownfish.Unwrap(d, "d"))
	retvalCF := C.LUCY_DataReader_Get_Segments(self);
	if retvalCF == nil {
		return nil
	}
	size := C.CFISH_Vec_Get_Size(retvalCF)
	retval := make([]Segment, int(size))
	for i := 0; i < int(size); i++ {
		elem := unsafe.Pointer(C.CFISH_Vec_Fetch(retvalCF, C.size_t(i)))
		retval[i] = clownfish.ToGo(unsafe.Pointer(C.cfish_incref(elem))).(Segment)
	}
	return retval
}

func (d *DataReaderIMP) Close() error {
	return clownfish.TrapErr(func() {
		self := (*C.lucy_DataReader)(clownfish.Unwrap(d, "d"))
		C.LUCY_DataReader_Close(self)
	})
}

func (d *DocReaderIMP) ReadDoc(docID int32, doc interface{}) error {
	self := (*C.lucy_DocReader)(clownfish.Unwrap(d, "d"))
	class := clownfish.GetClass(d)
	classC := ((*C.cfish_Class)(clownfish.Unwrap(class, "class")))
	if classC == C.LUCY_DEFAULTDOCREADER {
		return doReadDocData((*C.lucy_DefaultDocReader)(self), docID, doc)
	} else if classC == C.LUCY_POLYDOCREADER {
		return readDocPolyDR((*C.lucy_PolyDocReader)(self), docID, doc)
	} else {
		panic(clownfish.NewErr(fmt.Sprintf("Unexpected type: %s", class.GetName)))
	}
}

func (d *DocReaderIMP) FetchDoc(docID int32) (doc HitDoc, err error) {
	err = clownfish.TrapErr(func() {
		self := (*C.lucy_DocReader)(clownfish.Unwrap(d, "d"))
		docC := C.LUCY_DocReader_Fetch_Doc(self, C.int32_t(docID))
		doc = WRAPHitDoc(unsafe.Pointer(docC))
	})
	return doc, err
}

func (lr *LexiconReaderIMP) Lexicon(field string, term interface{}) (retval Lexicon, err error) {
	err = clownfish.TrapErr(func() {
		self := (*C.lucy_LexiconReader)(clownfish.Unwrap(lr, "lr"))
		fieldC := (*C.cfish_String)(clownfish.GoToClownfish(field, unsafe.Pointer(C.CFISH_STRING), false))
		defer C.cfish_decref(unsafe.Pointer(fieldC))
		termC := (*C.cfish_Obj)(clownfish.GoToClownfish(term, unsafe.Pointer(C.CFISH_OBJ), true))
		defer C.cfish_decref(unsafe.Pointer(termC))
		retvalCF := C.LUCY_LexReader_Lexicon(self, fieldC, termC)
		if retvalCF != nil {
			retval = clownfish.ToGo(unsafe.Pointer(retvalCF)).(Lexicon)
		}
	})
	return retval, err
}

func (lr *LexiconReaderIMP) DocFreq(field string, term interface{}) (retval uint32, err error) {
	err = clownfish.TrapErr(func() {
		self := (*C.lucy_LexiconReader)(clownfish.Unwrap(lr, "lr"))
		fieldC := (*C.cfish_String)(clownfish.GoToClownfish(field, unsafe.Pointer(C.CFISH_STRING), false))
		defer C.cfish_decref(unsafe.Pointer(fieldC))
		termC := (*C.cfish_Obj)(clownfish.GoToClownfish(term, unsafe.Pointer(C.CFISH_OBJ), true))
		defer C.cfish_decref(unsafe.Pointer(termC))
		retval = uint32(C.LUCY_LexReader_Doc_Freq(self, fieldC, termC))
	})
	return retval, err
}

func (lr *LexiconReaderIMP) fetchTermInfo(field string, term interface{}) (retval TermInfo, err error) {
	err = clownfish.TrapErr(func() {
		self := (*C.lucy_LexiconReader)(clownfish.Unwrap(lr, "lr"))
		fieldC := (*C.cfish_String)(clownfish.GoToClownfish(field, unsafe.Pointer(C.CFISH_STRING), false))
		defer C.cfish_decref(unsafe.Pointer(fieldC))
		termC := (*C.cfish_Obj)(clownfish.GoToClownfish(term, unsafe.Pointer(C.CFISH_OBJ), true))
		defer C.cfish_decref(unsafe.Pointer(termC))
		retvalCF := C.LUCY_LexReader_Fetch_Term_Info(self, fieldC, termC)
		if retvalCF != nil {
			retval = clownfish.ToGo(unsafe.Pointer(retvalCF)).(TermInfo)
		}
	})
	return retval, err
}

func (p *PostingListReaderIMP) PostingList(field string, term interface{}) (retval PostingList, err error) {
	err = clownfish.TrapErr(func() {
		self := (*C.lucy_PostingListReader)(clownfish.Unwrap(p, "p"))
		fieldC := (*C.cfish_String)(clownfish.GoToClownfish(field, unsafe.Pointer(C.CFISH_STRING), false))
		defer C.cfish_decref(unsafe.Pointer(fieldC))
		termC := (*C.cfish_Obj)(clownfish.GoToClownfish(term, unsafe.Pointer(C.CFISH_OBJ), true))
		defer C.cfish_decref(unsafe.Pointer(termC))
		retvalCF := C.LUCY_PListReader_Posting_List(self, fieldC, termC)
		if retvalCF != nil {
			retval = clownfish.ToGo(unsafe.Pointer(retvalCF)).(PostingList)
		}
	})
	return retval, err
}

func (h *HighlightReaderIMP) FetchDocVec(docID int32) (retval DocVector, err error) {
	err = clownfish.TrapErr(func() {
		self := (*C.lucy_HighlightReader)(clownfish.Unwrap(h, "h"))
		retvalCF := C.LUCY_HLReader_Fetch_Doc_Vec(self, C.int32_t(docID))
		retval = WRAPDocVector(unsafe.Pointer(retvalCF))
	})
	return retval, err
}

func (s *SortReaderIMP) fetchSortCache(field string) (retval SortCache, err error) {
	err = clownfish.TrapErr(func() {
		self := (*C.lucy_SortReader)(clownfish.Unwrap(s, "s"))
		fieldC := (*C.cfish_String)(clownfish.GoToClownfish(field, unsafe.Pointer(C.CFISH_STRING), false))
		defer C.cfish_decref(unsafe.Pointer(fieldC))
		retvalCF := C.LUCY_SortReader_Fetch_Sort_Cache(self, fieldC)
		if retvalCF != nil {
			retval = clownfish.ToGo(unsafe.Pointer(C.cfish_incref(unsafe.Pointer(retvalCF)))).(SortCache)
		}
	})
	return retval, err
}

func OpenIndexReader(index interface{}, snapshot Snapshot, manager IndexManager) (retval IndexReader, err error) {
	err = clownfish.TrapErr(func() {
		indexC := (*C.cfish_Obj)(clownfish.GoToClownfish(index, unsafe.Pointer(C.CFISH_OBJ), false))
		defer C.cfish_decref(unsafe.Pointer(indexC))
		snapshotC := (*C.lucy_Snapshot)(clownfish.UnwrapNullable(snapshot))
		managerC := (*C.lucy_IndexManager)(clownfish.UnwrapNullable(manager))
		cfObj := C.lucy_IxReader_open(indexC, snapshotC, managerC)
		retval = clownfish.WRAPAny(unsafe.Pointer(cfObj)).(IndexReader)
	})
	return retval, err
}

func (r *IndexReaderIMP) SegReaders() []SegReader {
	self := (*C.lucy_IndexReader)(clownfish.Unwrap(r, "r"))
	retvalCF := C.LUCY_IxReader_Seg_Readers(self);
	defer C.cfish_decref(unsafe.Pointer(retvalCF))
	if retvalCF == nil {
		return nil
	}
	size := C.CFISH_Vec_Get_Size(retvalCF)
	retval := make([]SegReader, int(size))
	for i := 0; i < int(size); i++ {
		elem := unsafe.Pointer(C.CFISH_Vec_Fetch(retvalCF, C.size_t(i)))
		retval[i] = clownfish.ToGo(unsafe.Pointer(C.cfish_incref(elem))).(SegReader)
	}
	return retval
}

func (r *IndexReaderIMP) Offsets() []int32 {
	self := (*C.lucy_IndexReader)(clownfish.Unwrap(r, "r"))
	retvalCF := C.LUCY_IxReader_Offsets(self)
	defer C.cfish_decref(unsafe.Pointer(retvalCF))
	return i32ArrayToSlice(retvalCF)
}

func (r *IndexReaderIMP) Obtain(api string) (retval DataReader, err error) {
	err = clownfish.TrapErr(func() {
		self := (*C.lucy_IndexReader)(clownfish.Unwrap(r, "r"))
		apiC := (*C.cfish_String)(clownfish.GoToClownfish(api, unsafe.Pointer(C.CFISH_STRING), false))
		defer C.cfish_decref(unsafe.Pointer(apiC))
		retvalCF := C.LUCY_IxReader_Obtain(self, apiC)
		retval = clownfish.WRAPAny(unsafe.Pointer(C.cfish_incref(unsafe.Pointer(retvalCF)))).(DataReader)
	})
	return retval, err
}
