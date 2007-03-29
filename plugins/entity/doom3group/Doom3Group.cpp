#include "Doom3Group.h"

#include "iregistry.h"
#include "selectable.h"
#include "render.h"
#include "transformlib.h"

namespace entity {

	namespace {
		const std::string RKEY_FREE_MODEL_ROTATION = "user/ui/freeModelRotation";
	}

inline void PointVertexArray_testSelect(PointVertex* first, std::size_t count, 
	SelectionTest& test, SelectionIntersection& best) 
{
	test.TestLineStrip(
	    VertexPointer(
	        reinterpret_cast<VertexPointer::pointer>(&first->vertex),
	        sizeof(PointVertex)
	    ),
	    IndexPointer::index_type(count),
	    best
	);
}

Doom3Group::Doom3Group(IEntityClassPtr eclass, scene::Node& node, 
		const Callback& transformChanged, 
		const Callback& boundsChanged, 
		const Callback& evaluateTransform) :
	m_entity(eclass),
	m_originKey(OriginChangedCaller(*this)),
	m_origin(ORIGINKEY_IDENTITY),
	m_nameOrigin(0,0,0),
	m_rotationKey(RotationChangedCaller(*this)),
	m_named(m_entity),
	m_nameKeys(m_entity),
	m_funcStaticOrigin(m_traverse, m_origin),
	m_renderOrigin(m_nameOrigin),
	m_renderName(m_named, m_nameOrigin),
	m_skin(SkinChangedCaller(*this)),
	m_transformChanged(transformChanged),
	m_evaluateTransform(evaluateTransform),
	m_traversable(0),
	m_curveNURBS(boundsChanged),
	m_curveCatmullRom(boundsChanged)
{
	construct();
}

Doom3Group::Doom3Group(const Doom3Group& other, scene::Node& node, 
		const Callback& transformChanged, 
		const Callback& boundsChanged, 
		const Callback& evaluateTransform) :
	m_entity(other.m_entity),
	m_originKey(OriginChangedCaller(*this)),
	m_origin(ORIGINKEY_IDENTITY),
	m_nameOrigin(0,0,0),
	m_rotationKey(RotationChangedCaller(*this)),
	m_named(m_entity),
	m_nameKeys(m_entity),
	m_funcStaticOrigin(m_traverse, m_origin),
	m_renderOrigin(m_nameOrigin),
	m_renderName(m_named, m_nameOrigin),
	m_skin(SkinChangedCaller(*this)),
	m_transformChanged(transformChanged),
	m_evaluateTransform(evaluateTransform),
	m_traversable(0),
	m_curveNURBS(boundsChanged),
	m_curveCatmullRom(boundsChanged)
{
	construct();
}

Doom3Group::~Doom3Group() {
	destroy();
}

void Doom3Group::instanceAttach(const scene::Path& path) {
	if (++m_instanceCounter.m_count == 1) {
		m_entity.instanceAttach(path_find_mapfile(path.begin(), path.end()));
		m_traverse.instanceAttach(path_find_mapfile(path.begin(), path.end()));
	}
}

void Doom3Group::instanceDetach(const scene::Path& path) {
	if (--m_instanceCounter.m_count == 0) {
		m_traverse.instanceDetach(path_find_mapfile(path.begin(), path.end()));
		m_entity.instanceDetach(path_find_mapfile(path.begin(), path.end()));
	}
}

EntityKeyValues& Doom3Group::getEntity() {
	return m_entity;
}
const EntityKeyValues& Doom3Group::getEntity() const {
	return m_entity;
}

scene::Traversable& Doom3Group::getTraversable() {
	return *m_traversable;
}

Namespaced& Doom3Group::getNamespaced() {
	return m_nameKeys;
}

Nameable& Doom3Group::getNameable() {
	return m_named;
}

TransformNode& Doom3Group::getTransformNode() {
	return m_transform;
}

ModelSkin& Doom3Group::getModelSkin() {
	return m_skin.get();
}

void Doom3Group::attach(scene::Traversable::Observer* observer) {
	m_traverseObservers.attach(*observer);
}

void Doom3Group::detach(scene::Traversable::Observer* observer) {
	m_traverseObservers.detach(*observer);
}

const AABB& Doom3Group::localAABB() const {
	m_curveBounds = m_curveNURBS.m_bounds;
	m_curveBounds.includeAABB(m_curveCatmullRom.m_bounds);
	return m_curveBounds;
}

void Doom3Group::addOriginToChildren() {
	m_funcStaticOrigin.addOriginToChildren();
}

void Doom3Group::removeOriginFromChildren() {
	m_funcStaticOrigin.removeOriginFromChildren();
}

void Doom3Group::renderSolid(Renderer& renderer, const VolumeTest& volume, 
	const Matrix4& localToWorld, bool selected) const 
{
	if (selected) {
		m_renderOrigin.render(renderer, volume, localToWorld);
	}

	renderer.SetState(m_entity.getEntityClass()->getWireShader(), Renderer::eWireframeOnly);
	renderer.SetState(m_entity.getEntityClass()->getWireShader(), Renderer::eFullMaterials);

	if (!m_curveNURBS.m_renderCurve.m_vertices.empty()) {
		renderer.addRenderable(m_curveNURBS.m_renderCurve, localToWorld);
	}
	if (!m_curveCatmullRom.m_renderCurve.m_vertices.empty()) {
		renderer.addRenderable(m_curveCatmullRom.m_renderCurve, localToWorld);
	}
}

void Doom3Group::renderWireframe(Renderer& renderer, const VolumeTest& volume, 
	const Matrix4& localToWorld, bool selected) const 
{
	renderSolid(renderer, volume, localToWorld, selected);
	if (GlobalRegistry().get("user/ui/xyview/showEntityNames") == "1") {
		renderer.addRenderable(m_renderName, localToWorld);
	}
}

void Doom3Group::testSelect(Selector& selector, SelectionTest& test, SelectionIntersection& best) {
	PointVertexArray_testSelect(&m_curveNURBS.m_renderCurve.m_vertices[0], m_curveNURBS.m_renderCurve.m_vertices.size(), test, best);
	PointVertexArray_testSelect(&m_curveCatmullRom.m_renderCurve.m_vertices[0], m_curveCatmullRom.m_renderCurve.m_vertices.size(), test, best);
}

void Doom3Group::translate(const Vector3& translation, bool rotation) {
	
	bool freeModelRotation = GlobalRegistry().get(RKEY_FREE_MODEL_ROTATION) == "1";
	
	// greebo: If the translation does not originate from 
	// a pivoted rotation, translate the origin as well (this is a bit hacky)
	// This also applies for models, which should always have the 
	// rotation-translation applied (except for freeModelRotation set to TRUE)
	if (!rotation || (isModel() && !freeModelRotation)) {
		m_origin = origin_translated(m_originKey.m_origin, translation);
	}
	
	// Only non-models should have their rendered origin different than <0,0,0>
	if (!isModel()) {
		m_nameOrigin = m_origin;
	}
	m_renderOrigin.updatePivot();
	translateChildren(translation);
}

void Doom3Group::rotate(const Quaternion& rotation) {
	if (!isModel()) {
		if (m_traversable != NULL) {
			m_traversable->traverse(ChildRotator(rotation));
		}
	}
	else {
		rotation_rotate(m_rotation, rotation);
	}
}

void Doom3Group::snapto(float snap) {
	m_originKey.m_origin = origin_snapped(m_originKey.m_origin, snap);
	m_originKey.write(&m_entity);
}

void Doom3Group::revertTransform() {
	m_origin = m_originKey.m_origin;
	
	// Only non-models should have their origin different than <0,0,0>
	if (!isModel()) {
		m_nameOrigin = m_origin;
	}
	else {
		rotation_assign(m_rotation, m_rotationKey.m_rotation);
	}
	
	m_renderOrigin.updatePivot();
	m_curveNURBS.m_controlPointsTransformed = m_curveNURBS.m_controlPoints;
	m_curveCatmullRom.m_controlPointsTransformed = m_curveCatmullRom.m_controlPoints;
}

void Doom3Group::freezeTransform() {
	m_originKey.m_origin = m_origin;
	m_originKey.write(&m_entity);
	
	if (!isModel()) {
		if (m_traversable != NULL) {
			m_traversable->traverse(ChildTransformFreezer());
		}
	}
	else {
		rotation_assign(m_rotationKey.m_rotation, m_rotation);
		m_rotationKey.write(&m_entity, isModel());
	}
	m_curveNURBS.m_controlPoints = m_curveNURBS.m_controlPointsTransformed;
	ControlPoints_write(m_curveNURBS.m_controlPoints, curve_Nurbs, m_entity);
	m_curveCatmullRom.m_controlPoints = m_curveCatmullRom.m_controlPointsTransformed;
	ControlPoints_write(m_curveCatmullRom.m_controlPoints, curve_CatmullRomSpline, m_entity);
}

void Doom3Group::transformChanged() {
	// If this is a container, pass the call to the children and leave the entity unharmed
	if (!isModel()) {
		if (m_traversable != NULL) {
			m_traversable->traverse(ChildTransformReverter());
			m_evaluateTransform();
		}
	}
	else {
		// It's a model
		revertTransform();
		m_evaluateTransform();
		updateTransform();
		m_curveNURBS.curveChanged();
		m_curveCatmullRom.curveChanged();
	}
}

void Doom3Group::construct() {
	default_rotation(m_rotation);

	m_keyObservers.insert("name", NamedEntity::IdentifierChangedCaller(m_named));
	m_keyObservers.insert("model", Doom3Group::ModelChangedCaller(*this));
	m_keyObservers.insert("origin", OriginKey::OriginChangedCaller(m_originKey));
	m_keyObservers.insert("angle", RotationKey::AngleChangedCaller(m_rotationKey));
	m_keyObservers.insert("rotation", RotationKey::RotationChangedCaller(m_rotationKey));
	m_keyObservers.insert("name", NameChangedCaller(*this));
	m_keyObservers.insert(curve_Nurbs, NURBSCurve::CurveChangedCaller(m_curveNURBS));
	m_keyObservers.insert(curve_CatmullRomSpline, CatmullRomSpline::CurveChangedCaller(m_curveCatmullRom));
	m_keyObservers.insert("skin", ModelSkinKey::SkinChangedCaller(m_skin));

	m_traverseObservers.attach(m_funcStaticOrigin);
	m_isModel = false;
	m_nameKeys.setKeyIsName(keyIsNameDoom3Doom3Group);
	attachTraverse();
	m_funcStaticOrigin.enable();

	m_entity.attach(m_keyObservers);
}

void Doom3Group::destroy() {
	m_entity.detach(m_keyObservers);

	if (isModel()) {
		detachModel();
	}
	else {
		detachTraverse();
	}

	m_traverseObservers.detach(m_funcStaticOrigin);
}

void Doom3Group::attachModel() {
	m_traversable = &m_model.getTraversable();
	m_model.attach(&m_traverseObservers);
}
void Doom3Group::detachModel() {
	m_traversable = 0;
	m_model.detach(&m_traverseObservers);
}

void Doom3Group::attachTraverse() {
	// greebo: Make this class a TraversableNodeSet, 
	// the getTraversable() call now retrieves the NodeSet
	m_traversable = &m_traverse;
	// Attach the TraverseObservers to the TraversableNodeSet, 
	// so that they get notified
	m_traverse.attach(&m_traverseObservers);
}

void Doom3Group::detachTraverse() {
	m_traversable = 0;
	m_traverse.detach(&m_traverseObservers);
}

bool Doom3Group::isModel() const {
	return m_isModel;
}

void Doom3Group::setIsModel(bool newValue) {
	if (newValue && !m_isModel) {
		m_funcStaticOrigin.disable();
		detachTraverse();
		attachModel();

		// The model key is not recognised as "name"
		m_nameKeys.setKeyIsName(keyIsNameDoom3);
		m_model.modelChanged(m_modelKey.c_str());
	}
	else if (!newValue && m_isModel) {
		// This is no longer a model, make it a TraversableNodeSet
		detachModel();
		attachTraverse();
		m_funcStaticOrigin.enable();

		// The model key should be recognised as "name" (important for "namespacing")
		m_nameKeys.setKeyIsName(keyIsNameDoom3Doom3Group);
	}
	m_isModel = newValue;
	updateTransform();
}

/** Determine if this Doom3Group is a model (func_static) or a
 * brush-containing entity. If the "model" key is equal to the
 * "name" key, then this is a brush-based entity, otherwise it is
 * a model entity. The exception to this is for the "worldspawn"
 * entity class, which is always a brush-based entity.
 */
void Doom3Group::updateIsModel() {
	if (m_modelKey != m_name && std::string(m_entity.getKeyValue("classname")) != "worldspawn") {
		setIsModel(true);
	}
	else {
		setIsModel(false);
	}
}

void Doom3Group::nameChanged(const char* value) {
	m_name = value;
	updateIsModel();
}

void Doom3Group::modelChanged(const char* value) {
	m_modelKey = value;
	updateIsModel();
	if (isModel()) {
		m_model.modelChanged(value);
	}
	else {
		m_model.modelChanged("");
	}
}

void Doom3Group::updateTransform() {
	m_transform.localToParent() = g_matrix4_identity;
	if (isModel()) {
		matrix4_translate_by_vec3(m_transform.localToParent(), m_origin);
		matrix4_multiply_by_matrix4(m_transform.localToParent(), rotation_toMatrix(m_rotation));
	}
	
	// Notify the InstanceSet of the Doom3GroupNode about this transformation change	 
	m_transformChanged();
	
	if (!isModel()) {
		//m_funcStaticOrigin.originChanged();
	}
}

void Doom3Group::translateChildren(const Vector3& childTranslation) {
	if (m_instanceCounter.m_count > 0 && m_traversable != NULL) {
		m_traversable->traverse(ChildTranslator(childTranslation));
	}
}

void Doom3Group::originChanged() {
	m_origin = m_originKey.m_origin;
	updateTransform();
	// Only non-models should have their origin different than <0,0,0>
	if (!isModel()) {
		m_nameOrigin = m_origin;
	}
	m_renderOrigin.updatePivot();
}

void Doom3Group::rotationChanged() {
	rotation_assign(m_rotation, m_rotationKey.m_rotation);
	updateTransform();
}

void Doom3Group::skinChanged() {
	if (isModel()) {
		scene::Node* node = m_model.getNode();
		if (node != 0) {
			Node_modelSkinChanged(*node);
		}
	}
}

} // namespace entity
